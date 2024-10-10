import h5py
import time
import numpy as np
from tqdm import tqdm
import os
from crpropa import *
import sys

# Constants
c_light = 299792458  # speed of light in m/s
lc = 1 * Mpc
lMin = 1.8 * lc
lMax = 2.20455205 * lc
Brms = 1e-7 * nG
sIndex = 5. / 3.
grid_num = 550
B_scale = (lMin / 2 * grid_num)

def split_energy_bins(energy_centers, parts=5):
    """Split the energy centers into specified number of parts."""
    total_energies = len(energy_centers)
    part_size = total_energies // parts
    return [energy_centers[i:i + part_size] for i in range(0, total_energies, part_size)]

def generate_log_bins(min_energy, max_energy, bins_per_decade):
    decade_count = np.log10(max_energy / min_energy)
    return np.logspace(np.log10(min_energy), np.log10(max_energy), int(decade_count * bins_per_decade) + 1)

def calculate_bin_centers(bins):
    return 10 ** (np.log10(bins[:-1]) + np.diff(np.log10(bins)) / 2)
def setup_magnetic_field(dsrc):
    randomSeed = 42
    obs_pos = dsrc - B_scale
    turbSpectrum = SimpleTurbulenceSpectrum(Brms, lMin, lMax, sIndex)
    gridprops = GridProperties(Vector3d(obs_pos), grid_num, lMin / 2)
    B_field = SimpleGridTurbulence(turbSpectrum, gridprops, randomSeed)
    return B_field

def run_simulation(Energy, B_field, source_dsrc, obs_pos, output_filename):
    thinning = 0.4
    sim = ModuleList()
    # sim.add(SimplePropagation())
    sim.add(PropagationCK(B_field))
    sim.add(MaximumTrajectoryLength(2 * B_scale))
    sim.add(Redshift())
    sim.add(SynchrotronRadiation(B_field, True, thinning))
    sim.add(EMPairProduction(CMB(), True, thinning))
    sim.add(EMPairProduction(IRB_Dominguez11(), True, thinning))
    sim.add(EMInverseComptonScattering(CMB(), True, thinning))
    sim.add(EMInverseComptonScattering(IRB_Dominguez11(), True, thinning))
    sim.add(MinimumEnergy(1 * GeV))
    obs = Observer()
    obs.add(Observer1D(obs_pos))
    output = HDF5Output(output_filename, Output.Event3D)
    output.setEnergyScale(eV)
    output.enable(output.WeightColumn)
    output.enable(output.CurrentIdColumn)
    output.enable(output.CurrentPositionColumn)
    output.enable(output.CurrentDirectionColumn)
    output.enable(output.CandidateTagColumn)
    output.enable(output.TrajectoryLengthColumn)
    output.enable(output.RedshiftColumn)
    obs.onDetection(output)
    sim.add(obs)

    source = Source()
    source.add(SourcePosition(Vector3d(source_dsrc, 0, 0)))
    source.add(SourceRedshift1D())
    source.add(SourceParticleType(11))
    source.add(SourceEnergy(Energy * eV))
    sim.setShowProgress(True)
    sim.run(source, 500, True, False)

    output.close()

    sim.end()

    if not os.path.exists(output_filename) or os.stat(output_filename).st_size == 0:
        with h5py.File(output_filename, 'w') as file:
            file.create_group("CRPROPA3")
            print(f"Empty HDF5 file created: {output_filename}")
    else:
        print(f"Results saved in: {output_filename}")

    return


def run_simulation_at_redshift_energy(target_z, target_energy, redshifts):
    # Find the target index in the redshift list
    try:
        target_index = redshifts.index(target_z)
    except ValueError:
        print("Target redshift not in the list.")
        return

    if target_index == 0:
        print("No previous redshift available for the first element.")
        return

    # Set the source at the previous redshift and observer at the current
    source_z = redshifts[target_index - 1]

    obs_z = target_z

    source_dsrc = redshift2ComovingDistance(source_z)
    obs_dsrc = redshift2ComovingDistance(obs_z)

    # Setup magnetic field and observer position
    B_field = setup_magnetic_field(obs_dsrc)
    # Format energy string to remove dots and use 'e' notation
    energy_str = f"{target_energy:.0e}".replace("e+", "e").replace(".", "")
    # Format redshift string to remove dots and maintain precision
    redshift_str = f"{target_z:.2f}".replace(".", "")

    output_filename = f"/Users/eobardthawne/Desktop/Icetube/crp_scripts/sim_withB/h5results/z{redshift_str}_E{energy_str}.h5"

    # Run the simulation
    run_simulation(target_energy, B_field, source_dsrc, obs_dsrc, output_filename)
    print(
        f"Simulation step from z={source_z:.5f} to z={target_z:.5f} at E={target_energy:.2e} eV completed and saved to {output_filename}")

def main(part_index=0):
    start_time = time.time()  # Start timing the entire simulation

    energy_bins = generate_log_bins(1e9, 1e16, 10)  # From 1 GeV to 10 PeV
    energy_centers = calculate_bin_centers(energy_bins)
    energy_parts = split_energy_bins(energy_centers, 20)  # Split into 20 parts
    chosen_energies = energy_parts[part_index]  # Select part by index

    redshifts = [2.0, 1.8, 1.6, 1.4, 1.2, 1.0, 0.95, 0.9, 0.85, 0.8, 0.75, 0.7, 0.65, 0.6, 0.55, 0.5, 0.45, 0.4, 0.35,
                 0.3, 0.28473684, 0.26947368, 0.25421053, 0.23894737, 0.22368421, 0.20842105, 0.19315789, 0.17789474,
                 0.16263158, 0.14736842, 0.13210526, 0.11684211, 0.10157895, 0.08631579, 0.07105263, 0.05578947,
                 0.04052632, 0.02526316, 0.01, 0]

    for energy in tqdm(chosen_energies, desc="Processing Energies"):
        energy_start_time = time.time()  # Time the processing of each energy level
        for z in tqdm(redshifts[1:], desc=f"Simulating at Energy {energy:.2e} eV"):
            run_simulation_at_redshift_energy(z, energy, redshifts)
        energy_end_time = time.time()  # End timing for each energy level
        print(f"Time taken for Energy {energy:.2e} eV: {energy_end_time - energy_start_time:.2f} seconds")

    end_time = time.time()  # End timing the entire simulation
    print(f"Total simulation time: {end_time - start_time:.2f} seconds")

if __name__ == "__main__":
    if len(sys.argv) > 1:
        part_index = int(sys.argv[1])  # Convert command line argument to integer
    else:
        print('Wrong index!')  # Default part index if none provided
    main(part_index)
    # redshifts = [2.0, 1.8, 1.6, 1.4, 1.2, 1.0, 0.95, 0.9, 0.85, 0.8, 0.75, 0.7, 0.65, 0.6, 0.55, 0.5, 0.45, 0.4, 0.35,
    #              0.3, 0.28473684, 0.26947368, 0.25421053, 0.23894737, 0.22368421, 0.20842105, 0.19315789, 0.17789474,
    #              0.16263158, 0.14736842, 0.13210526, 0.11684211, 0.10157895, 0.08631579, 0.07105263, 0.05578947,
    #              0.04052632, 0.02526316, 0.01, 0]
    # run_simulation_at_redshift_energy(0.9, 1.12201845e+14, redshifts)