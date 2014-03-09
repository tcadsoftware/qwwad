/**
 * \file dos-functions.cpp Functions for calculating density of states
 *
 * \author Paul Harrison  <p.harrison@shu.ac.uk>
 * \author Alex Valavanis <a.valavanis@leeds.ac.uk>
 */

#include "dos-functions.h"
#include "qclsim-constants.h"
#include <cmath>

using namespace Leeds;
using namespace constants;

/**
 * Calculate bulk density of states
 *
 * \param[in] mass   The effective mass of the carrier at the band edge [kg]
 * \param[in] energy The energy of the carrier above the band edge [J]
 * \param[in] V      Band edge potential [J]
 * \param[in] alpha  The nonparabolicity factor [1/J]
 *
 * \returns The density of states [J^{-1}m^{-3}]
 *
 * \details The effective mass is assumed to take the form
 *          \f[
 *          m^*(E) = m_0[1 + \alpha(E-V)]
 *          \f]
 */
double calculate_dos_3D(const double mass,
                        const double energy,
                        const double V,
                        const double alpha)
{
    double rho = 0.0;

    if(gsl_fcmp(energy,V,energy/1e12)==1)
    {
        // Bulk density of states [QWWAD3, Eq. 2.40]
        rho = gsl_pow_3(sqrt(2*mass/(hBar*hBar)))*sqrt(energy-V)/(2*gsl_pow_2(pi)) * (1.0 + alpha*(2.0*energy-V));
    }

    return rho;
}

/**
 * Calculate 2D density of states for a quantum well
 *
 * \param[in] mass       The effective mass of the carrier [kg]
 * \param[in] E_carrier  The energy of the carrier [J]
 * \param[in] E_subbands The energies of the minima of each subband in the well [J]
 * \param[in] V          Band edge potential [J]
 * \param[in] alpha      The nonparabolicity factor [1/J]
 *
 * \returns The density of states [J^{-1}m^{-2}]
 *
 * \details The effective mass is assumed to take the form
 *          \f[
 *          m^*(E) = m_0[1 + \alpha(E-V)]
 *          \f]
 */
double calculate_dos_2D(const double                 mass,
                        const double                 E_carrier,
                        const std::valarray<double> &E_subbands,
                        const double                 V,
                        const double                 alpha)
{
    // Density of states in a single subband [QWWAD3, Eq. 2.46]
    const double dos_1sb = mass/(pi*gsl_pow_2(hBar)) + mass*alpha*(2*E_carrier - V)/(pi*gsl_pow_2(hBar));

    double dos_total = 0; // Total dos over all occupied subbands
    const size_t nsb = E_subbands.size();

    // Loop over subbands
    for(unsigned int isb = 0; isb < nsb; ++isb)
    {
        // Increment dos whenever we enter a new subband
        // [QWWAD3, Eq. 2.47]
        if(E_carrier > E_subbands[isb])
            dos_total += dos_1sb;
    }

    return dos_total;
}

/**
 * Calculate 1D density of states for a quantum wire
 *
 * \param[in] mass       The effective mass of the carrier [kg]
 * \param[in] E_carrier  The energy of the carrier [J]
 * \param[in] E_subbands The energies of the minima of each subband in the well [J]
 *
 * \returns The density of states [J^{-1}m^{-1}]
 *
 * \details It is assumed that carriers have a parabolic dispersion
 */
double calculate_dos_1D(const double                 mass,
                        const double                 E_carrier,
                        const std::valarray<double> &E_subbands)
{
    double dos_total = 0; // Total dos over all occupied subbands
    const size_t nsb = E_subbands.size();

    // Loop over subbands
    for(unsigned int isb = 0; isb < nsb; ++isb)
    {
        // Increment dos whenever we enter a new subband
        if(E_carrier > E_subbands[isb])
            dos_total += sqrt(2*mass)/hBar/(pi*sqrt(E_carrier - E_subbands[isb]));
    }

    return dos_total;
}
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:fileencoding=utf-8:textwidth=99 :
