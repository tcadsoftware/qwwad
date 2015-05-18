/**
 * \file   srelo.cpp
 * \brief  Scattering Rate Electron-LO phonon
 * \author Paul Harrison  <p.harrison@shu.ac.uk>
 * \author Alex Valavanis <a.valavanis@leeds.ac.uk>
 */

#include <complex>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <iostream>
#include <gsl/gsl_math.h>
#include "qwwad/constants.h"
#include "qwwad/file-io.h"
#include "qclsim-subband.h"
#include "qwwad-options.h"

using namespace QWWAD;
using namespace constants;

static void ff_table(const double   dKz,
                     const Subband &isb,
                     const Subband &fsb,
                     unsigned int   nKz,
                     std::valarray<double> &Kz,
                     std::valarray<double> &Gifsqr);

void ff_output(const std::valarray<double> &Kz,
               const std::valarray<double> &Gifsqr,
               unsigned int        i,
               unsigned int        f);

static double Gsqr(const double   Kz,
                   const Subband &isb,
                   const Subband &fsb);

static Options configure_options(int argc, char* argv[])
{
    Options opt;

    std::string doc("Find the polar LO-phonon scattering rate.");

    opt.add_option<bool>  ("outputff,a",            "Output form-factors to file.");
    opt.add_option<bool>  ("noblocking,b",          "Disable final-state blocking.");
    opt.add_option<bool>  ("noscreening,S",         "Disable screening.");
    opt.add_option<double>("latticeconst,A",  5.65, "Lattice constant in growth direction [angstrom]");
    opt.add_option<double>("ELO,E",          36.0,  "Energy of LO phonon [meV]");
    opt.add_option<double>("epss,e",         13.18, "Static dielectric constant");
    opt.add_option<double>("epsinf,f",       10.89, "High-frequency dielectric constant");
    opt.add_option<double>("mass,m",         0.067, "Band-edge effective mass (relative to free electron)");
    opt.add_option<char>  ("particle,p",       'e', "ID of particle to be used: 'e', 'h' or 'l', for "
                                                    "electrons, heavy holes or light holes respectively.");
    opt.add_option<double>("Te",               300, "Carrier temperature [K].");
    opt.add_option<double>("Tl",               300, "Lattice temperature [K].");
    opt.add_option<double>("Ecutoff",               "Cut-off energy for carrier distribution [meV]. If not specified, then 5kT above band-edge.");
    opt.add_option<size_t>("nki",             1001, "Number of initial wave-vector samples.");
    opt.add_option<size_t>("nKz",             1001, "Number of phonon wave-vector samples.");

    opt.add_prog_specific_options_and_parse(argc, argv, doc);

    return opt;
}

int main(int argc,char *argv[])
{
    const auto opt = configure_options(argc, argv);

    const auto ff_flag     =  opt.get_option<bool>  ("outputff");             // True if formfactors are wanted
    const auto A0          =  opt.get_option<double>("latticeconst") * 1e-10; // Lattice constant [m]
    const auto Ephonon     =  opt.get_option<double>("ELO") * e/1000;         // Phonon energy [J]
    const auto epsilon_s   =  opt.get_option<double>("epss")   * eps0;        // Static permittivity [F/m]
    const auto epsilon_inf =  opt.get_option<double>("epsinf") * eps0;        // High-frequency permittivity [F/m]
    const auto m           =  opt.get_option<double>("mass")*me;              // Band-edge effective mass [kg]
    const auto p           =  opt.get_option<char>  ("particle");             // Particle ID
    const auto Te          =  opt.get_option<double>("Te");                   // Carrier temperature [K]
    const auto Tl          =  opt.get_option<double>("Tl");                   // Lattice temperature [K]
    const auto b_flag      = !opt.get_option<bool>  ("noblocking");           // Include final-state blocking by default
    const auto S_flag      = !opt.get_option<bool>  ("noscreening");          // Include screening by default
    const auto nki         =  opt.get_option<size_t>("nki");                  // number of ki calculations
    const auto nKz         =  opt.get_option<size_t>("nKz");                  // number of Kz calculations

// calculate step length in phonon wave-vector
const double dKz=2/(A0*nKz); // Taken range of phonon integration as 2/A0

// calculate often used constants
const double omega_0 = Ephonon/hBar; // phonon angular frequency
const double N0      = 1.0/(exp(Ephonon/(kB*Tl))-1.0); // Bose-Einstein factor

// Find pre-factors for scattering rates
const double Upsilon_star_a=pi*e*e*omega_0/epsilon_s*(epsilon_s/epsilon_inf-1)*(N0)
    *2*m/gsl_pow_2(hBar)*2/(8*pi*pi*pi);

const double Upsilon_star_e=pi*e*e*omega_0/epsilon_s*(epsilon_s/epsilon_inf-1)*(N0+1)
    *2*m/gsl_pow_2(hBar)*2/(8*pi*pi*pi);

    std::ostringstream E_filename; // Energy filename string
    E_filename << "E" << p << ".r";
    std::ostringstream wf_prefix;  // Wavefunction filename prefix
    wf_prefix << "wf_" << p;

    // Read data for all subbands from file
    std::vector<Subband> subbands = Subband::read_from_file(E_filename.str(),
            wf_prefix.str(),
            ".r",
            m);

// Read and set carrier distributions within each subband
std::valarray<double>       Ef;      // Fermi energies [J]
std::valarray<double>       N;       // Subband populations [m^{-2}]
std::valarray<unsigned int> indices; // Subband indices (garbage)
read_table("Ef.r", indices, Ef);
Ef *= e/1000.0; // Rescale to J
read_table("N.r", N);	// read populations

for(unsigned int isb = 0; isb < subbands.size(); ++isb)
    subbands[isb].set_distribution(Ef[isb], N[isb]);

// Read list of wanted transitions
std::valarray<unsigned int> i_indices;
std::valarray<unsigned int> f_indices;

read_table("rrp.r", i_indices, f_indices);
const size_t ntx = i_indices.size();

double lambda_s_sq = 0.0; // Screening length squared [m^2]

if(S_flag)
{
    // Sum over all subbands
    for(std::vector<Subband>::const_iterator jsb = subbands.begin(); jsb != subbands.end(); ++jsb)
    {
        const double Ej = jsb->get_E();
        lambda_s_sq += sqrt(2*m*Ej) * m * jsb->f_FD(Ej,Te);
    }
    lambda_s_sq *= e*e/(pi*pi*hBar*hBar*hBar*epsilon_s);
}

std::valarray<double> Wabar(ntx);
std::valarray<double> Webar(ntx);

// Loop over all desired transitions
for(unsigned int itx = 0; itx < i_indices.size(); ++itx)
{
    // State indices for this transition (NB., these are indexed from 1)
    unsigned int i = i_indices[itx];
    unsigned int f = f_indices[itx];

    // Convenience labels for each subband (NB., these are indexed from 0)
    const Subband isb = subbands[i-1];
    const Subband fsb = subbands[f-1];

    // Subband minima
    const double Ei = isb.get_E();
    const double Ef = fsb.get_E();

    double kimax = 0;
    double Ecutoff = 0.0; // Maximum kinetic energy in initial subband

    // Use user-specified value if given
    if(opt.vm.count("Ecutoff"))
    {
        Ecutoff = opt.get_option<double>("Ecutoff")*e/1000;

        if(Ecutoff+Ei - Ephonon < Ef)
        {
            std::cerr << "No scattering permitted from state " << i << "->" << f << " within the specified cut-off energy." << std::endl;
            std::cerr << "Extending range automatically" << std::endl;
            Ecutoff += Ef;
        }
    }
    // Otherwise, use a fixed, 5kT range
    else
    {
        kimax   = isb.get_k_max(Te);
        Ecutoff = hBar*hBar*kimax*kimax/(2*m);

        if(Ecutoff+Ei < Ef)
            Ecutoff += Ef;
    }

    kimax = isb.k(Ecutoff);

    std::valarray<double> Kz(nKz);
    std::valarray<double> Gifsqr(nKz);

    ff_table(dKz,isb,fsb,nKz,Kz,Gifsqr); // generates formfactor table

    // Output form-factors if desired
    if(ff_flag)
        ff_output(Kz, Gifsqr, i,f);

    /* Generate filename for particular mechanism and open file	*/
    char	filename[9];	/* character string for output filename		*/
    sprintf(filename,"LOa%i%i.r",i,f);	/* absorption	*/
    FILE *FLOa=fopen(filename,"w");			
    sprintf(filename,"LOe%i%i.r",i,f);	/* emission	*/
    FILE *FLOe=fopen(filename,"w");			

 /* calculate Delta variables, constant for each mechanism	*/
 const double Delta_a = Ef - Ei - Ephonon;
 const double Delta_e = Ef - Ei + Ephonon;

 /* calculate maximum value of ki and hence ki step length */
 const double dki=kimax/((float)nki); // Step length for integration over initial wave-vector [1/m]

 std::valarray<double> Waif(nki); // Absorption scattering rate at this wave-vector [1/s]
 std::valarray<double> Weif(nki); // Emission scattering rate at this wave-vector [1/s]

 std::valarray<double> Wabar_integrand_ki(nki); // Average scattering rate [1/s]
 std::valarray<double> Webar_integrand_ki(nki); // Average scattering rate [1/s]

 // calculate e-LO rate for all ki
 for(unsigned int iki=0;iki<nki;iki++)
 {
  const double ki=dki*iki;
  std::valarray<double> Waif_integrand_dKz(nKz); // Integrand for scattering rate
  std::valarray<double> Weif_integrand_dKz(nKz); // Integrand for scattering rate

  // Integral over phonon wavevector Kz
  for(unsigned int iKz=0;iKz<nKz;iKz++)
  {
      double Kz_2 = Kz[iKz] * Kz[iKz];

      // Apply screening if wanted
      if(S_flag && iKz != 0)
          Kz_2 *= (1.0 + 2*lambda_s_sq/Kz_2 + lambda_s_sq*lambda_s_sq/(Kz_2*Kz_2));

      const double Kz_4 = Kz_2 * Kz_2;

      Waif_integrand_dKz[iKz] = Gifsqr[iKz] /
          sqrt(Kz_4 + 2*Kz_2 * (2*ki*ki - 2*m*Delta_a/(hBar*hBar))+
                  gsl_pow_2(2*m*Delta_a/(hBar*hBar)));

      Weif_integrand_dKz[iKz] = Gifsqr[iKz] /
          sqrt(Kz_4 + 2*Kz_2 * (2*ki*ki - 2*m*Delta_e/(hBar*hBar))+
                  gsl_pow_2(2*m*Delta_e/(hBar*hBar))
              );
  } // end integral over Kz

  // Note integral from 0->inf, hence *	2
  Waif[iki] = Upsilon_star_a*pi*integral(Waif_integrand_dKz,dKz);
  Weif[iki] = Upsilon_star_e*pi*integral(Weif_integrand_dKz,dKz);

  const double Eki = isb.Ek(ki); // Initial kinetic energy

  // Final kinetic energy
  const double Ef_em = Eki - Delta_e;
  const double Ef_ab = Eki - Delta_a;

  /* Now check for energy conservation!, would be faster with a nasty `if'
     statement just after the beginning of the ki loop!			*/
  Weif[iki] *= Theta(Ef_em);
  Waif[iki] *= Theta(Ef_ab);

  // Include final-state blocking factor
  if (b_flag)
  {
      // Final wave-vector
      if(Ef_em >= 0)
      {
          const double kf_em = sqrt(Ef_em*2*m)/hBar;
          Weif[iki] *= (1.0 - fsb.f_FD_k(kf_em, Te));
      }

      if(Ef_ab >= 0)
      {
          const double kf_ab = sqrt(Ef_ab*2*m)/hBar;
          Waif[iki] *= (1.0 - fsb.f_FD_k(kf_ab, Te));
      }
  }

  Wabar_integrand_ki[iki] = Waif[iki]*ki*isb.f_FD_k(ki, Te);
  Webar_integrand_ki[iki] = Weif[iki]*ki*isb.f_FD_k(ki, Te);

  /* output scattering rate versus carrier energy=subband minima+in-plane
     kinetic energy						*/
  fprintf(FLOa,"%20.17le %20.17le\n",(Ei + Eki)/(1e-3*e),Waif[iki]);
  fprintf(FLOe,"%20.17le %20.17le\n",(Ei + Eki)/(1e-3*e),Weif[iki]);
 } // End loop over ki

 Wabar[itx] = integral(Wabar_integrand_ki, dki)/(pi*isb.get_pop());
 Webar[itx] = integral(Webar_integrand_ki, dki)/(pi*isb.get_pop());

 fclose(FLOa);	/* close output file for this mechanism	*/
 fclose(FLOe);	/* close output file for this mechanism	*/
} /* end while over states */

write_table("LOa-if.r", i_indices, f_indices, Wabar);
write_table("LOe-if.r", i_indices, f_indices, Webar);

return EXIT_SUCCESS;
}

/**
 * \brief Computes the formfactor at a range of phonon wave-vectors
 */
static void ff_table(const double   dKz,
                     const Subband &isb,
                     const Subband &fsb,
                     unsigned int   nKz,
                     std::valarray<double> &Kz,
                     std::valarray<double> &Gifsqr)
{
    for(unsigned int iKz=0;iKz<nKz;iKz++)
    {
        Kz[iKz]     = iKz*dKz;                 // Magnitude of phonon wave vector
        Gifsqr[iKz] = Gsqr(Kz[iKz], isb, fsb); // Squared form-factor
    }
}

/**
 * \brief calculates the overlap integral squared between the two states
 */
static double Gsqr(const double   Kz,
                   const Subband &isb,
                   const Subband &fsb)
{
 const std::valarray<double> z = isb.z_array();
 const double dz = z[1] - z[0];
 const double nz = z.size();
 const std::valarray<double> psi_i = isb.psi_array();
 const std::valarray<double> psi_f = fsb.psi_array();

 std::complex<double> I(0,1); // Imaginary unit

 // Find form-factor integral
 std::valarray< std::complex<double> > G_integrand_dz(nz);

 for(unsigned int iz=0; iz<nz; ++iz)
     G_integrand_dz[iz] = exp(Kz*z[iz]*I) * psi_i[iz] * psi_f[iz];

 std::complex<double> G = integral(G_integrand_dz, dz);

 return norm(G);
}

/* This function outputs the formfactors into files	*/
void ff_output(const std::valarray<double> &Kz,
               const std::valarray<double> &Gifsqr,
               unsigned int        i,
               unsigned int        f)
{
 char	filename[9];	/* output filename				*/
 sprintf(filename,"G%i%i.r",i,f);	
 write_table(filename, Kz, Gifsqr);
}
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:fileencoding=utf-8:textwidth=99 :
