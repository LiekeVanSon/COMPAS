#include "HeHG.h"
#include "HeGB.h"
#include "HeWD.h"


/*
 * Calculate timescales in units of Myr
 *
 * Timescales depend on a star's mass, so this needs to be called at least each timestep
 *
 * Vectors are passed by reference here for performance - preference would be to pass const& and
 * pass modified value back by functional return, but this way is faster - and this function is
 * called many, many times.
 *
 *
 * void CalculateTimescales(const double p_Mass, DBL_VECTOR &p_Timescales)
 *
 * @param   [IN]        p_Mass                  Mass in Msol
 * @param   [IN/OUT]    p_Timescales            Timescales
 */
void HeHG::CalculateTimescales(const double p_Mass, DBL_VECTOR &p_Timescales) {
#define timescales(x) p_Timescales[static_cast<int>(TIMESCALE::x)]  // for convenience and readability - undefined at end of function
#define gbParams(x) m_GBParams[static_cast<int>(GBP::x)]            // for convenience and readability - undefined at end of function

    HeMS::CalculateTimescales(p_Mass, p_Timescales);    // calculate common values

    double p1   = gbParams(p) - 1.0;
    double q1   = gbParams(q) - 1.0;
    double p1_p = p1 / gbParams(p);
    double q1_q = q1 / gbParams(q);

    double LTHe = HeMS::CalculateLuminosityAtPhaseEnd(p_Mass);

    timescales(tinf1_HeGB) = timescales(tHeMS) + (1.0 / ((p1 * gbParams(AHe) * gbParams(D))) * pow((gbParams(D) / LTHe), p1_p));
    timescales(tx_HeGB)    = timescales(tinf1_HeGB) - (timescales(tinf1_HeGB) - (timescales(tHeMS) * pow((LTHe / gbParams(Lx)), p1_p)));
    timescales(tinf2_HeGB) = timescales(tx_HeGB) + ((1.0 / (q1 * gbParams(AHe) * gbParams(B))) * pow((gbParams(B) / gbParams(Lx)), q1_q));

#undef gbParams
#undef timescales
}


/*
 * Calculate Giant Branch (GB) parameters per Hurley et al. 2000
 *
 * Giant Branch Parameters depend on a star's mass, so this needs to be called at least each timestep
 *
 * Vectors are passed by reference here for performance - preference would be to pass const& and
 * pass modified value back by functional return, but this way is faster - and this function is
 * called many, many times.
 *
 * void CalculateGBParams(const double p_Mass, DBL_VECTOR &p_GBParams)
 *
 * @param   [IN]        p_Mass                  Mass in Msol
 * @param   [IN/OUT]    p_GBParams              Giant Branch Parameters - calculated here
 */
void HeHG::CalculateGBParams(const double p_Mass, DBL_VECTOR &p_GBParams) {
#define gbParams(x) p_GBParams[static_cast<int>(GBP::x)]    // for convenience and readability - undefined at end of function

    GiantBranch::CalculateGBParams(p_Mass, p_GBParams);                         // calculate common values (actually, all)

    // recalculate HeHG specific values

	gbParams(B)      = CalculateCoreMass_Luminosity_B_Static();
	gbParams(D)      = CalculateCoreMass_Luminosity_D_Static(p_Mass);

	gbParams(McBAGB) = CalculateCoreMassAtBAGB();
	gbParams(McBGB)  = CalculateCoreMassAtBGB(p_Mass, p_GBParams);

    gbParams(McSN)   = CalculateCoreMassAtSupernova_Static(gbParams(McBAGB));   // JR: Added this

#undef gbParams
}


/*
 * Calculate luminosity for a Helium HertzSprung Gap star
 *
 * Uses Helium Giant Branch luminosity
 *
 *
 * double CalculateLuminosityOnPhase_Static()
 *
 * @return                                      Luminosity for a Helium HertzSprung Gap star
 */
double HeHG::CalculateLuminosityOnPhase() {
#define gbParams(x) m_GBParams[static_cast<int>(GBP::x)]    // for convenience and readability - undefined at end of function

    return HeGB::CalculateLuminosityOnPhase_Static(m_COCoreMass, gbParams(B), gbParams(D));

#undef gbParams
}


/*
 * Calculate radius of a Helium HertzSprung Gap star
 *
 * Uses Helium Giant Branch radius
 *
 *
 * double CalculateRadiusOnPhase()
 *
 * @return                                      Radius of a Helium HertzSprung Gap star
 */
double HeHG::CalculateRadiusOnPhase() {

    double R1, R2;
    std::tie(R1, R2) = HeGB::CalculateRadiusOnPhase_Static(m_Mass, m_Luminosity);

    return (utils::Compare(R1, R2) < 0) ? R1 : R2;
}


/*
 * Calculate CO Core Mass for a Helium Hertzsprung Gap star
 *
 *
 * double CalculateCOCoreMassOnPhase()
 *
 * @return                                      HeHG CoCoreMass in Msol
 */
double HeHG::CalculateCOCoreMassOnPhase() {
#define timescales(x) m_Timescales[static_cast<int>(TIMESCALE::x)]  // for convenience and readability - undefined at end of function

    return HeGB::CalculateCoreMassOnPhase_Static(m_Mass0, m_Age, timescales(tHeMS), m_GBParams);

#undef timescales
}


/*
 * Calculate rejuvenation factor for stellar age based on mass lost/gained during mass transfer
 *
 * JR: Description?
 *
 * Always returns 1.0 for HeHG - the rejuvenation factor is unity for convective main sequence stars.
 * The rest of the code is here so sanity checks can be made and an error displayed if a bad prescription
 * was specified in the program options
 *
 *
 * double CalculateMassTransferRejuvenationFactor()
 *
 * @return                                      Rejuvenation factor
 */
double HeHG::CalculateMassTransferRejuvenationFactor() {

    double fRej = 1.0;

    switch (OPTIONS->MassTransferRejuvenationPrescription()) {          // which rejuvenation prescription?

        case MT_REJUVENATION_PRESCRIPTION::NONE:                        // use default Hurley et al. 2000 prescription = 1.0
        case MT_REJUVENATION_PRESCRIPTION::STARTRACK:                   // StarTrack 2008 prescription - section 5.6 of http://arxiv.org/pdf/astro-ph/0511811v3.pdf

            if (utils::Compare(m_Mass, m_MassPrev) <= 0) {              // Rejuvenation factor is unity for mass losing stars
                fRej = 1.0;
            }
            break;

        default:                                                        // unknow prescription - use default Hurley et al. 2000 prescription = 1.0
            SHOW_WARN(ERROR::UNKNOWN_MT_REJUVENATION_PRESCRIPTION);     // show warning
    }

    return fRej;
}


/*
 * Calculate the perturbation parameter mu
 *
 * Hurley et al. 2000, eqs 97 & 98
 *
 *
 * double CalculatePerturbationMu()
 *
 * @return                                      Perturbation parameter mu
 */
double HeHG::CalculatePerturbationMu() {
    double McMax = CalculateMaximumCoreMass(m_Mass);
    return 5.0 * ((McMax - m_CoreMass) / McMax);
}


///////////////////////////////////////////////////////////////////////////////////////
//                                                                                   //
//                                LAMBDA CALCULATIONS                                //
//                                                                                   //
///////////////////////////////////////////////////////////////////////////////////////


/*
 * Calculate the common envelope lambda parameter using the "Nanjing" prescription
 * from X.-J. Xu and X.-D. Li arXiv:1004.4957 (v1, 28Apr2010) as implemented in STARTRACK
 *
 * This implementation adapted from the STARTRACK implementation (STARTRACK courtesy Chris Belczynski)
 *
 * This function good for HeHG and HeGB stars (for Helium stars: always use Natasha's fit)
 *
 *
 * double CalculateLambdaNanjing()
 *
 * @return                                      Nanjing lambda for use in common envelope
 */
double HeHG::CalculateLambdaNanjing() {

    constexpr double rMin = 0.25;                              // minimum considered radius: Natasha       JR: todo: should this be in constants.h?
	constexpr double rMax = 120.0;                             // maximum considered radius: Natasha       JR: todo: should this be in constants.h?

	constexpr double rMinLambda = 0.3 * pow(rMin, -0.8);       // JR: todo: should this be in constants.h?
	constexpr double rMaxLambda = 0.3 * pow(rMax, -0.8);       // JR: todo: should this be in constants.h?

	return m_Radius < rMin ? rMinLambda : (m_Radius > rMax ? rMaxLambda : 0.3 * pow(m_Radius, -0.8));
}


///////////////////////////////////////////////////////////////////////////////////////
//                                                                                   //
//                    MISCELLANEOUS FUNCTIONS / CONTROL FUNCTIONS                    //
//                                                                                   //
///////////////////////////////////////////////////////////////////////////////////////


/*
 * Determines if mass transfer produces a wet merger
 *
 * According to the mass ratio limit discussed by de Mink et al. 2013 and Claeys et al. 2014
 *
 * Assumes this star is the donor; relevant accretor details are passed as parameters
 *
 *
 * bool IsMassRatioUnstable(const double p_AccretorMass, const bool p_AccretorIsDegenerate)
 *
 * @param   [IN]    p_AccretorMass              Mass of accretor in Msol
 * @param   [IN]    p_AccretorIsDegenerate      Boolean indicating if accretor in degenerate (true = degenerate)
 * @return                                      Boolean indicating stability of mass transfer (true = unstable)
 */
bool HeHG::IsMassRatioUnstable(const double p_AccretorMass, const bool p_AccretorIsDegenerate) {

    bool result = false;                                                                                                    // default is stable

    if (OPTIONS->MassTransferCriticalMassRatioHeliumHG()) {
        result = p_AccretorIsDegenerate
                    ? (p_AccretorMass / m_Mass) < OPTIONS->MassTransferCriticalMassRatioHeliumHGDegenerateAccretor()        // degenerate accretor
                    : (p_AccretorMass / m_Mass) < OPTIONS->MassTransferCriticalMassRatioHeliumHGNonDegenerateAccretor();    // non-degenerate accretor
    }

    return result;
}

/*
 * Choose timestep for evolution
 *
 * Can obviously do this your own way
 * Given in the discussion in Hurley et al. 2000
 *
 *
 * ChooseTimestep(const double p_Time)
 *
 * @param   [IN]    p_Time                      Current age of star in Myr
 * @return                                      Suggested timestep (dt)
 */
double HeHG::ChooseTimestep(const double p_Time) {
#define timescales(x) m_Timescales[static_cast<int>(TIMESCALE::x)]  // for convenience and readability - undefined at end of function

    double dtk = utils::Compare(p_Time, timescales(tx_HeGB)) > 0
                    ? 0.02 * (timescales(tinf2_SAGB) - p_Time)
                    : 0.02 * (timescales(tinf1_SAGB) - p_Time);
    double dte = dtk;

    return std::max(dte, NUCLEAR_MINIMUM_TIMESTEP);

#undef timescales
}


/*
 * Determine if evolution should continue on this phase, or whether evolution
 * on this phase should end (and so evolve to next phase)
 *
 *
 * bool ShouldEvolveOnPhase()
 *
 * @return                                      Boolean flag: true if evolution on this phase should continue, false if not
 */
bool HeHG::ShouldEvolveOnPhase() {
    double McMax = CalculateMaximumCoreMass(m_Mass);
    return utils::Compare(m_COCoreMass, McMax) <= 0 || utils::Compare(McMax, CalculateMaximumCoreMassSN()) >= 0;    // Evolve on HeHG phase if McCO <= McMax or McMax >= McSN
}


/*
 * Modify the star after it loses its envelope
 *
 * Hurley et al. 2000, section 6 just before eq 76
 *
 *
 * STELLAR_TYPE ResolveEnvelopeLoss()
 *
 * @return                                      Stellar Type to which star shoule evolve after losing envelope
 */
STELLAR_TYPE HeHG::ResolveEnvelopeLoss(bool p_NoCheck) {

    STELLAR_TYPE stellarType = m_StellarType;

    if (p_NoCheck || utils::Compare(m_COCoreMass, m_Mass) > 0) {        // Envelope lost - determine what type of star to form

        stellarType = (utils::Compare(m_COCoreMass, 1.6) < 0) ? STELLAR_TYPE::CARBON_OXYGEN_WHITE_DWARF : STELLAR_TYPE::OXYGEN_NEON_WHITE_DWARF;

        m_CoreMass  = m_COCoreMass;
        m_Mass      = m_CoreMass;
        m_Mass0     = m_Mass;
        m_EnvMass   = 0.0;
        m_Age       = 0.0;
        m_Radius    = HeWD::CalculateRadiusOnPhase_Static(m_Mass);
    }

    return stellarType;
}


/*
 * Set parameters for evolution to next phase and return Stellar Type for next phase
 *
 *
 * STELLAR_TYPE EvolveToNextPhase()
 *
 * @return                                      Stellar Type for next phase
 */
STELLAR_TYPE HeHG::EvolveToNextPhase() {
    return STELLAR_TYPE::CARBON_OXYGEN_WHITE_DWARF;
}