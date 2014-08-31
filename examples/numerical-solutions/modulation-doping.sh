#!/bin/sh
set -e

# Define output file
outfile=modulation-doping-E-I.dat

# Initialise files
rm -f $outfile wf_e1*.r

# First generate structure definition `s.r' file
echo 100 0.2 2e17 > s.r
echo 100 0.0 0.0  >> s.r
echo 100 0.2 2e17 >> s.r
 
find_heterostructure	# generate alloy concentration as a function of z
efxv			# generate potential data

cp v.r vcb.r # Save conduction-band energy
  
for I in 0 1 2 3; do
 # Calculate ground state Schroedinger solution
 efss --nst-max 1

 densityinput # Generate an estimate of the population density
 chargedensity # Compute charge density profile

 # save wave function in separate file
 cp wf_e1.r wf_e1-I=$I.r

 # Write energy to output file
 E1=`awk '{printf("\t%20.17e\n",$2)}' Ee.r`

 echo $I $E1 >> $outfile

 # Implement self consistent Poisson calculation
 find_poisson_potential --Vbasefile vcb.r --potential-file v.r
done # X