import numpy as np
import h5py  as h5
import os
import matplotlib.pyplot as plt
from astropy.cosmology import WMAP9 as cosmology
from scipy.interpolate import interp1d
import ClassCOMPAS
import selection_effects

Z0       = 0.035
alpha    = -0.23
sigma    = 0.39
maxRedshift=10.0
stepRedshift=0.001
GWdetector_sensitivity='O1'
GWdetector_snrThreshold=8
SFRmassPerSampledBinary=115 #Msol; amount of star formation for each generated binary, to be adjusted depending on sampling boundaries [this is for [5,150] Msol with binary fraction of 0.7, would be 90 for binary fraction of 1

pathData        = "/Users/ilyam/Work/COMPASresults/popsynth/JeffTest/"
COMPAS = ClassCOMPAS.COMPASData(path=pathData,Mlower=5,Mupper=150,binaryFraction=0.7)
COMPAS.setGridAndMassEvolved()
COMPAS.setCOMPASDCOmask(types='BBH', pessimistic=True)
COMPAS.setCOMPASData()

nrRedshiftBins = int(maxRedshift/stepRedshift)
ageFirstSFR = cosmology.age(maxRedshift).value

redshifts   =  np.linspace(0, maxRedshift, nrRedshiftBins)
times       = cosmology.age(redshifts).value
redshiftsFromTimes = interp1d(times, redshifts)
distances   = cosmology.luminosity_distance(redshifts).value  #in Mpc
distances[0]= 0.001 #avoid division by 0
volumes     = cosmology.comoving_volume(redshifts).value*1e-9  #cumulative comoving volume, Gpc^3
shellVolumes= np.diff(volumes)
shellVolumes= np.append(shellVolumes, shellVolumes[-1])  #add last element again to keep same array length

SFR = 0.01 * ((1+redshifts)**2.77) / (1 + ((1+redshifts)/2.9)**4.7) * 1e9  #per Gpc^3 per year
SFRfactor=SFR/(SFRmassPerSampledBinary*len(COMPAS.initialSeeds))

Zmean=Z0 * 10**(alpha*redshifts)
Zmu=np.log(Zmean)-sigma**2/2
steplogZ=0.01
nrZbins=int(12.0/steplogZ)
logZvector=np.linspace(-12.0,0.0,nrZbins) # in natural logZ
Zvector=np.exp(logZvector)
dPdlogZ=1/sigma/np.sqrt(2*np.pi)*np.exp(-(logZvector-Zmu[:,np.newaxis])**2/2/sigma**2)
norm=dPdlogZ.sum(axis=1)*steplogZ
dPdlogZ=dPdlogZ/norm[:,np.newaxis]
pDrawZ=1/(np.log(max(COMPAS.metallicitySystems))-np.log(min(COMPAS.metallicitySystems)))

nrBinaries=len(COMPAS.delayTimes)
formationRate=np.zeros(shape=(nrBinaries,nrRedshiftBins))
mergerRate=np.zeros(shape=(nrBinaries,nrRedshiftBins))
detectionRate=np.zeros(shape=(nrBinaries,nrRedshiftBins))
for i in range(nrBinaries):
    formationRate[i,:]=SFRfactor*dPdlogZ[:,np.digitize(COMPAS.metallicitySystems[i],Zvector)]/pDrawZ
    tDelay=COMPAS.delayTimes[i]/1000      #convert to Gyr
    tForm=times-tDelay
    firstTooEarlyIndex=np.digitize(ageFirstSFR,tForm)
    if(firstTooEarlyIndex==nrRedshiftBins):     #if there are no values of tForm outside the times vector
        firstTooEarlyIndex=nrRedshiftBins+1
    if(firstTooEarlyIndex>0):
        zForm=redshiftsFromTimes(tForm[0:firstTooEarlyIndex-1])
        zFormIndex=np.ceil(zForm/stepRedshift)
        mergerRate[i,0:firstTooEarlyIndex-1]=formationRate[i,zFormIndex.astype(int)]
    pDetection=selection_effects.detection_probability(COMPAS.mass1[i],COMPAS.mass2[i], redshifts, distances,  GWdetector_snrThreshold, GWdetector_sensitivity)
    detectionRate[i,:]=mergerRate[i,:]*pDetection*shellVolumes/(1+redshifts)

totalFormationRate=np.sum(formationRate,axis=0)
totalMergerRate=np.sum(mergerRate,axis=0)
totalDetectionRate=np.sum(detectionRate,axis=0)
cumulativeDetectionRate=np.cumsum(totalDetectionRate)
detectionRateByBinary=np.sum(detectionRate,axis=1)
chirpMass=COMPAS.mass1**0.6*COMPAS.mass2**0.6/(COMPAS.mass1+COMPAS.mass2)**0.2

fig, axes = plt.subplots(2,2)
axes[0,0].plot(redshifts, totalFormationRate)
axes[0,0].set_xlabel('Redshift')
axes[0,0].set_ylabel('Formation rate [dN/dGpc^3/dyr]')
axes[0,1].plot(redshifts, totalMergerRate)
axes[0,1].set_xlabel('Redshift')
axes[0,1].set_ylabel('Merger rate [dN/dGpc^3/dyr]')
axes[1,0].plot(redshifts[redshifts<1], cumulativeDetectionRate[redshifts<1])
axes[1,0].set_xlabel('Redshift')
axes[1,0].set_ylabel('Cumulative detection rate [dN/dyr]')
axes[1,1].hist(chirpMass,weights=detectionRateByBinary,bins=20,range=(0,50))
axes[1,1].set_xlabel('Chirp mass, M_o')
axes[1,1].set_ylabel('Detected chirp mass distribution')
plt.show()
