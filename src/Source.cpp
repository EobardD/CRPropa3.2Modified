#include "crpropa/Source.h"
#include "crpropa/Random.h"
#include "crpropa/Cosmology.h"
#include "crpropa/Common.h"
#include "crpropa/Units.h"
#include "crpropa/ParticleID.h"

#ifdef CRPROPA_HAVE_MUPARSER
#include "muParser.h"
#endif

#include <sstream>
#include <stdexcept>

namespace crpropa {

// Source ---------------------------------------------------------------------
void Source::add(SourceFeature* property) {
	features.push_back(property);
}

ref_ptr<Candidate> Source::getCandidate() const {
	ref_ptr<Candidate> candidate = new Candidate();
	for (int i = 0; i < features.size(); i++)
		(*features[i]).prepareCandidate(*candidate);
	return candidate;
}

std::string Source::getDescription() const {
	std::stringstream ss;
	ss << "Cosmic ray source\n";
	for (int i = 0; i < features.size(); i++)
		ss << "    " << features[i]->getDescription();
	return ss.str();
}

// SourceList------------------------------------------------------------------
void SourceList::add(Source* source, double weight) {
	sources.push_back(source);
	if (cdf.size() > 0)
		weight += cdf.back();
	cdf.push_back(weight);
}

ref_ptr<Candidate> SourceList::getCandidate() const {
	if (sources.size() == 0)
		throw std::runtime_error("SourceList: no sources set");
	size_t i = Random::instance().randBin(cdf);
	return (sources[i])->getCandidate();
}

std::string SourceList::getDescription() const {
	std::stringstream ss;
	ss << "List of cosmic ray sources\n";
	for (int i = 0; i < sources.size(); i++)
		ss << "  " << sources[i]->getDescription();
	return ss.str();
}

// SourceFeature---------------------------------------------------------------
void SourceFeature::prepareCandidate(Candidate& candidate) const {
	ParticleState &source = candidate.source;
	prepareParticle(source);
	candidate.created = source;
	candidate.current = source;
	candidate.previous = source;
}

std::string SourceFeature::getDescription() const {
	return description;
}

// ----------------------------------------------------------------------------
SourceParticleType::SourceParticleType(int id) :
		id(id) {
	setDescription();
}

void SourceParticleType::prepareParticle(ParticleState& particle) const {
	particle.setId(id);
}

void SourceParticleType::setDescription() {
	std::stringstream ss;
	ss << "SourceParticleType: " << id << "\n";
	description = ss.str();
}

// ----------------------------------------------------------------------------
SourceMultipleParticleTypes::SourceMultipleParticleTypes() {
	setDescription();
}

void SourceMultipleParticleTypes::add(int id, double a) {
	particleTypes.push_back(id);
	if (cdf.size() > 0)
		a += cdf.back();
	cdf.push_back(a);
	setDescription();
}

void SourceMultipleParticleTypes::prepareParticle(ParticleState& particle) const {
	if (particleTypes.size() == 0)
		throw std::runtime_error("SourceMultipleParticleTypes: no nuclei set");
	size_t i = Random::instance().randBin(cdf);
	particle.setId(particleTypes[i]);
}

void SourceMultipleParticleTypes::setDescription() {
	std::stringstream ss;
	ss << "SourceMultipleParticleTypes: Random particle type\n";
	for (int i = 0; i < particleTypes.size(); i++)
		ss << "      ID = " << particleTypes[i] << "\n";
	description = ss.str();
}

// ----------------------------------------------------------------------------
SourceEnergy::SourceEnergy(double energy) :
		E(energy) {
	setDescription();
}

void SourceEnergy::prepareParticle(ParticleState& p) const {
	p.setEnergy(E);
}

void SourceEnergy::setDescription() {
	std::stringstream ss;
	ss << "SourceEnergy: " << E / EeV << " EeV\n";
	description = ss.str();
}

// ----------------------------------------------------------------------------
SourcePowerLawSpectrum::SourcePowerLawSpectrum(double Emin, double Emax,
		double index) :
		Emin(Emin), Emax(Emax), index(index) {
	setDescription();
}

void SourcePowerLawSpectrum::prepareParticle(ParticleState& particle) const {
	Random &random = Random::instance();
	double E = random.randPowerLaw(index, Emin, Emax);
	particle.setEnergy(E);
}

void SourcePowerLawSpectrum::setDescription() {
	std::stringstream ss;
	ss << "SourcePowerLawSpectrum: Random energy ";
	ss << "E = " << Emin / EeV << " - " << Emax / EeV << " EeV, ";
	ss << "dN/dE ~ E^" << index  << "\n";
	description = ss.str();
}

// ----------------------------------------------------------------------------
SourceComposition::SourceComposition(double Emin, double Rmax, double index) :
		Emin(Emin), Rmax(Rmax), index(index) {
	setDescription();
}

void SourceComposition::add(int id, double weight) {
	nuclei.push_back(id);
	int A = massNumber(id);
	int Z = chargeNumber(id);

	double a = 1 + index;
	if (std::abs(a) < std::numeric_limits<double>::min())
		weight *= log(Z * Rmax / Emin);
	else
		weight *= (pow(Z * Rmax, a) - pow(Emin, a)) / a;

	weight *= pow(A, -a);

	if (cdf.size() > 0)
		weight += cdf.back();
	cdf.push_back(weight);
	setDescription();
}

void SourceComposition::add(int A, int Z, double a) {
	add(nucleusId(A, Z), a);
}

void SourceComposition::prepareParticle(ParticleState& particle) const {
	if (nuclei.size() == 0)
		throw std::runtime_error("SourceComposition: No source isotope set");

	Random &random = Random::instance();

	// draw random particle type
	size_t i = random.randBin(cdf);
	int id = nuclei[i];
	particle.setId(id);

	// random energy from power law
	int Z = chargeNumber(id);
	particle.setEnergy(random.randPowerLaw(index, Emin, Z * Rmax));
}

void SourceComposition::setDescription() {
	std::stringstream ss;
	ss << "SourceComposition: Random element and energy ";
	ss << "E = " << Emin / EeV << " - Z*" << Rmax / EeV << " EeV, ";
	ss << "dN/dE ~ E^" << index << "\n";
	for (int i = 0; i < nuclei.size(); i++)
		ss << "      ID = " << nuclei[i] << "\n";
	description = ss.str();
}

// ----------------------------------------------------------------------------
SourcePosition::SourcePosition(Vector3d position) :
		position(position) {
	setDescription();
}

SourcePosition::SourcePosition(double d) :
		position(Vector3d(d, 0, 0)) {
	setDescription();
}

void SourcePosition::prepareParticle(ParticleState& particle) const {
	particle.setPosition(position);
}

void SourcePosition::setDescription() {
	std::stringstream ss;
	ss << "SourcePosition: " << position / Mpc << " Mpc\n";
	description = ss.str();
}

// ----------------------------------------------------------------------------
SourceMultiplePositions::SourceMultiplePositions() {
	setDescription();
}

void SourceMultiplePositions::add(Vector3d pos, double weight) {
	positions.push_back(pos);
	if (cdf.size() > 0)
		weight += cdf.back();
	cdf.push_back(weight);
}

void SourceMultiplePositions::prepareParticle(ParticleState& particle) const {
	if (positions.size() == 0)
		throw std::runtime_error("SourceMultiplePositions: no position set");
	size_t i = Random::instance().randBin(cdf);
	particle.setPosition(positions[i]);
}

void SourceMultiplePositions::setDescription() {
	std::stringstream ss;
	ss << "SourceMultiplePositions: Random position from list\n";
	for (int i = 0; i < positions.size(); i++)
		ss << "  " << positions[i] / Mpc << " Mpc\n";
	description = ss.str();
}

// ----------------------------------------------------------------------------
SourceUniformSphere::SourceUniformSphere(Vector3d center, double radius) :
		center(center), radius(radius) {
	setDescription();
}

void SourceUniformSphere::prepareParticle(ParticleState& particle) const {
	Random &random = Random::instance();
	double r = pow(random.rand(), 1. / 3.) * radius;
	particle.setPosition(center + random.randVector() * r);
}

void SourceUniformSphere::setDescription() {
	std::stringstream ss;
	ss << "SourceUniformSphere: Random position within a sphere at ";
	ss << center / Mpc << " Mpc with";
	ss  << radius / Mpc << " Mpc radius\n";
	description = ss.str();
}

// ----------------------------------------------------------------------------
SourceUniformHollowSphere::SourceUniformHollowSphere(
		Vector3d center,
		double radius_inner,
		double radius_outer) :
		center(center), radius_inner(radius_inner),
		radius_outer(radius_outer) {
	setDescription();
}

void SourceUniformHollowSphere::prepareParticle(ParticleState& particle) const {
	Random &random = Random::instance();
	double r = radius_inner + pow(random.rand(), 1. / 3.) * (radius_outer - radius_inner);
	particle.setPosition(center + random.randVector() * r);
}

void SourceUniformHollowSphere::setDescription() {
	std::stringstream ss;
	ss << "SourceUniformHollowSphere: Random position within a sphere at ";
	ss << center / Mpc << " Mpc with";
	ss << radius_inner / Mpc << " Mpc inner radius\n";
	ss << radius_outer / Mpc << " Mpc outer radius\n";
	description = ss.str();
}

// ----------------------------------------------------------------------------
SourceUniformShell::SourceUniformShell(Vector3d center, double radius) :
		center(center), radius(radius) {
	setDescription();
}

void SourceUniformShell::prepareParticle(ParticleState& particle) const {
	Random &random = Random::instance();
	particle.setPosition(center + random.randVector() * radius);
}

void SourceUniformShell::setDescription() {
	std::stringstream ss;
	ss << "SourceUniformShell: Random position on a spherical shell at ";
	ss << center / Mpc << " Mpc with ";
	ss << radius / Mpc << " Mpc radius\n";
	description = ss.str();
}

// ----------------------------------------------------------------------------
SourceUniformBox::SourceUniformBox(Vector3d origin, Vector3d size) :
		origin(origin), size(size) {
	setDescription();
}

void SourceUniformBox::prepareParticle(ParticleState& particle) const {
	Random &random = Random::instance();
	Vector3d pos(random.rand(), random.rand(), random.rand());
	particle.setPosition(pos * size + origin);
}

void SourceUniformBox::setDescription() {
	std::stringstream ss;
	ss << "SourceUniformBox: Random uniform position in box with ";
	ss << "origin = " << origin / Mpc << " Mpc and ";
	ss << "size = " << size / Mpc << " Mpc\n";
	description = ss.str();
}

// ---------------------------------------------------------------------------
SourceUniformCylinder::SourceUniformCylinder(Vector3d origin, double height, double radius) :
    origin(origin), height(height), radius(radius) {
}

void SourceUniformCylinder::prepareParticle(ParticleState& particle) const {
  Random &random = Random::instance();
  double phi = 2*M_PI*random.rand();
  double RandRadius = radius*pow(random.rand(), 1. / 2.);
  Vector3d pos(cos(phi)*RandRadius, sin(phi)*RandRadius, (-0.5+random.rand())*height);
  particle.setPosition(pos + origin);
  }

void SourceUniformCylinder::setDescription() {
	std::stringstream ss;
	ss << "SourceUniformCylinder: Random uniform position in cylinder with ";
	ss << "origin = " << origin / Mpc << " Mpc and ";
	ss << "radius = " << radius / Mpc << " Mpc and";
	ss << "height = " << height / Mpc << " Mpc\n";
	description = ss.str();
}

// ---------------------------------------------------------------------------
SourceSNRDistribution::SourceSNRDistribution() :
    rEarth(8.5 * kpc),alpha(2.), beta(3.53), zg(0.3 * kpc) {
	setFrMax();
	setFzMax(0.3 * kpc);
	setRMax(20 * kpc);
	setZMax(5 * kpc);
}

SourceSNRDistribution::SourceSNRDistribution(double rEarth, double alpha, double beta, double zg) :
    rEarth(rEarth),alpha(alpha), beta(beta), zg(zg) {
	setFrMax();
	setFzMax(zg);
	setRMax(20 * kpc);
	setZMax(5 * kpc);
}

void SourceSNRDistribution::prepareParticle(ParticleState& particle) const {
  	Random &random = Random::instance();
	double RPos;
	while (true) {
		RPos = random.rand() * rMax;
		double fTest = random.rand() * frMax;
		double fR = fr(RPos);
		if (fTest <= fR) {
			break;
		}
	}
	double ZPos;
	while (true) {
		ZPos = (random.rand() - 0.5) * 2 * zMax;
		double fTest = random.rand() * fzMax;
		double fZ=fz(ZPos);
		if (fTest<=fZ) {
			break;
		}
	}
	double phi = random.rand() * 2 * M_PI;
	Vector3d pos(cos(phi) * RPos, sin(phi) * RPos, ZPos);
	particle.setPosition(pos);
}

double SourceSNRDistribution::fr(double r) const {
	return pow(r / rEarth, alpha) * exp(- beta * (r - rEarth) / rEarth);
}

double SourceSNRDistribution::fz(double z) const{
	double Az = 1.;
	double f = 1. / zg * exp(- fabs(z) / zg);
	double fz = Az * f;
	return fz;
}

double SourceSNRDistribution::getAlpha() const {
	return alpha;
}

double SourceSNRDistribution::getBeta() const {
	return beta;
}

void SourceSNRDistribution::setFrMax() {
	frMax = pow(alpha / beta, alpha) * exp(beta - alpha);
	return;
}

void SourceSNRDistribution::setFzMax(double zg) {
	fzMax = 1. / zg;
	return;
}

void SourceSNRDistribution::setRMax(double r) {
	rMax = r;
	return;
}

void SourceSNRDistribution::setZMax(double z) {
	zMax = z;
	return;
}

double SourceSNRDistribution::getFrMax() const {
	return frMax;
}

double SourceSNRDistribution::getFzMax() const {
	return fzMax;
}

double SourceSNRDistribution::getRMax() const {
	return rMax;
}

double SourceSNRDistribution::getZMax() const {
	return zMax;
}

void SourceSNRDistribution::setAlpha(double a) {
	alpha = a;
	setRMax(rMax);
	setFrMax();
}

void SourceSNRDistribution::setBeta(double b) {
	beta = b;
	setRMax(rMax);
	setFrMax();
}

void SourceSNRDistribution::setDescription() {
	std::stringstream ss;
	ss << "SourceSNRDistribution: Random position according to SNR distribution";
	ss << "rEarth = " << rEarth / kpc << " kpc and ";
	ss << "zg = " << zg / kpc << " kpc and";
	ss << "beta = " << beta << " \n";
	description = ss.str();
}

// ---------------------------------------------------------------------------
SourcePulsarDistribution::SourcePulsarDistribution() :
    rEarth(8.5*kpc), beta(3.53), zg(0.3*kpc) {
	setFrMax(8.5*kpc, 3.53);
	setFzMax(0.3*kpc);
	setRMax(22*kpc);
	setZMax(5*kpc);
	setRBlur(0.07);
	setThetaBlur(0.35/kpc);
}

SourcePulsarDistribution::SourcePulsarDistribution(double rEarth, double beta, double zg, double rB, double tB) :
    rEarth(rEarth), beta(beta), zg(zg) {
	setFrMax(rEarth, beta);
	setFzMax(zg);
	setRBlur(rB);
	setThetaBlur(tB);
	setRMax(22 * kpc);
	setZMax(5 * kpc);
}

void SourcePulsarDistribution::prepareParticle(ParticleState& particle) const {
  	Random &random = Random::instance();
	double Rtilde;
	while (true) {
		Rtilde = random.rand() * rMax;
		double fTest = random.rand() * frMax;
		double fR = fr(Rtilde);
		if (fTest <= fR) {
			break;
		}
	}
	double ZPos;
	while (true) {
		ZPos = (random.rand() - 0.5) * 2 * zMax;
		double fTest = random.rand() * fzMax;
		double fZ = fz(ZPos);
		if (fTest <= fZ) {
			break;
		}
	}

	int i = random.randInt(3);
	double thetaTilde = ftheta(i, Rtilde);
	double RPos = blurR(Rtilde);
	double phi = blurTheta(thetaTilde, Rtilde);
	Vector3d pos(cos(phi) * RPos, sin(phi) * RPos, ZPos);

	particle.setPosition(pos);
  }

double SourcePulsarDistribution::fr(double r) const {
	double Atilde = (pow(beta, 4.) * exp(-beta)) / (12 * M_PI * pow(rEarth, 2.));
 	double f = pow(r / rEarth, 2.) * exp(-beta * (r - rEarth) / rEarth);
	double fr = Atilde * f;
	return fr;
}

double SourcePulsarDistribution::fz(double z) const{
	double Az = 1.;
	double f = 1. / zg * exp(- fabs(z) / zg);
	double fz = Az * f;
	return fz;
}

double SourcePulsarDistribution::ftheta(int i, double r) const {
	const double k_0[] = {4.25, 4.25, 4.89, 4.89};
	const double r_0[] = {3.48 * kpc, 3.48 * kpc, 4.9 * kpc, 4.9 * kpc};
	const double theta_0[] = {0., 3.14, 2.52, -0.62};
	double K = k_0[i];
	double R = r_0[i];
	double Theta = theta_0[i];

	double theta = K * log(r / R) + Theta;

	return theta;
}

double SourcePulsarDistribution::blurR(double rTilde) const {
	Random &random = Random::instance();
	return random.randNorm(rTilde, rBlur * rTilde);
}

double SourcePulsarDistribution::blurTheta(double thetaTilde, double rTilde) const {
	Random &random = Random::instance();
	double thetaCorr = (random.rand() - 0.5) * 2 * M_PI;
	double tau = thetaCorr * exp(- thetaBlur * rTilde);
	return thetaTilde + tau;
}

void SourcePulsarDistribution::setFrMax(double R, double b) {
	frMax = pow(b, 2.) / (3 * pow(R, 2.) * M_PI) * exp(-2.);
	return;
}

void SourcePulsarDistribution::setFzMax(double zg) {
	fzMax = 1. / zg;
	return;
}

void SourcePulsarDistribution::setRMax(double r) {
	rMax = r;
	return;
}

void SourcePulsarDistribution::setZMax(double z) {
	zMax = z;
	return;
}

void SourcePulsarDistribution::setRBlur(double r) {
	rBlur = r;
	return;
}

void SourcePulsarDistribution::setThetaBlur(double theta) {
	thetaBlur = theta;
	return;
}

double SourcePulsarDistribution::getFrMax() {
	return frMax;
}

double SourcePulsarDistribution::getFzMax() {
	return fzMax;
}

double SourcePulsarDistribution::getRMax() {
	return rMax;
}

double SourcePulsarDistribution::getZMax() {
	return zMax;
}

double SourcePulsarDistribution::getRBlur() {
	return rBlur;
}

double SourcePulsarDistribution::getThetaBlur() {
	return thetaBlur;
}


void SourcePulsarDistribution::setDescription() {
	std::stringstream ss;
	ss << "SourcePulsarDistribution: Random position according to pulsar distribution";
	ss << "rEarth = " << rEarth / kpc << " kpc and ";
	ss << "zg = " << zg / kpc << " kpc and ";
	ss << "beta = " << beta << " and ";
	ss << "r_blur = " << rBlur << " and ";
	ss << "theta blur = " << thetaBlur << "\n";
	description = ss.str();
}

// ----------------------------------------------------------------------------
SourceUniform1D::SourceUniform1D(double minD, double maxD, bool withCosmology) {
	this->withCosmology = withCosmology;
	if (withCosmology) {
		this->minD = comoving2LightTravelDistance(minD);
		this->maxD = comoving2LightTravelDistance(maxD);
	} else {
		this->minD = minD;
		this->maxD = maxD;
	}
	setDescription();
}

void SourceUniform1D::prepareParticle(ParticleState& particle) const {
	Random& random = Random::instance();
	double d = random.rand() * (maxD - minD) + minD;
	if (withCosmology)
		d = lightTravel2ComovingDistance(d);
	particle.setPosition(Vector3d(d, 0, 0));
}

void SourceUniform1D::setDescription() {
	std::stringstream ss;
	ss << "SourceUniform1D: Random uniform position in D = ";
	ss << minD / Mpc << " - " << maxD / Mpc << " Mpc";
	if (withCosmology)
		ss << " (including cosmology)";
	ss << "\n";
	description = ss.str();
}

// ----------------------------------------------------------------------------
SourceDensityGrid::SourceDensityGrid(ref_ptr<Grid1f> grid) :
		grid(grid) {
	float sum = 0;
	for (int ix = 0; ix < grid->getNx(); ix++) {
		for (int iy = 0; iy < grid->getNy(); iy++) {
			for (int iz = 0; iz < grid->getNz(); iz++) {
				sum += grid->get(ix, iy, iz);
				grid->get(ix, iy, iz) = sum;
			}
		}
	}
	setDescription();
}

void SourceDensityGrid::prepareParticle(ParticleState& particle) const {
	Random &random = Random::instance();

	// draw random bin
	size_t i = random.randBin(grid->getGrid());
	Vector3d pos = grid->positionFromIndex(i);

	// draw uniform position within bin
	double dx = random.rand() - 0.5;
	double dy = random.rand() - 0.5;
	double dz = random.rand() - 0.5;
	pos += Vector3d(dx, dy, dz) * grid->getSpacing();

	particle.setPosition(pos);
}

void SourceDensityGrid::setDescription() {
	description = "SourceDensityGrid: 3D source distribution according to density grid\n";
}

// ----------------------------------------------------------------------------
SourceDensityGrid1D::SourceDensityGrid1D(ref_ptr<Grid1f> grid) :
		grid(grid) {
	if (grid->getNy() != 1)
		throw std::runtime_error("SourceDensityGrid1D: Ny != 1");
	if (grid->getNz() != 1)
		throw std::runtime_error("SourceDensityGrid1D: Nz != 1");

	float sum = 0;
	for (int ix = 0; ix < grid->getNx(); ix++) {
		sum += grid->get(ix, 0, 0);
		grid->get(ix, 0, 0) = sum;
	}
	setDescription();
}

void SourceDensityGrid1D::prepareParticle(ParticleState& particle) const {
	Random &random = Random::instance();

	// draw random bin
	size_t i = random.randBin(grid->getGrid());
	Vector3d pos = grid->positionFromIndex(i);

	// draw uniform position within bin
	double dx = random.rand() - 0.5;
	pos.x += dx * grid->getSpacing().x;

	particle.setPosition(pos);
}

void SourceDensityGrid1D::setDescription() {
	description = "SourceDensityGrid1D: 1D source distribution according to density grid\n";
}

// ----------------------------------------------------------------------------
SourceIsotropicEmission::SourceIsotropicEmission() {
	setDescription();
}

void SourceIsotropicEmission::prepareParticle(ParticleState& particle) const {
	Random &random = Random::instance();
	particle.setDirection(random.randVector());
}

void SourceIsotropicEmission::setDescription() {
	description = "SourceIsotropicEmission: Random isotropic direction\n";
}

// ----------------------------------------------------------------------------
SourceDirectedEmission::SourceDirectedEmission(Vector3d mu, double kappa): mu(mu), kappa(kappa) {
	if (kappa <= 0)
		throw std::runtime_error("The concentration parameter kappa should be larger than 0.");
	setDescription();
}

void SourceDirectedEmission::prepareCandidate(Candidate &candidate) const {
	Random &random = Random::instance();

	Vector3d muvec = mu / mu.getR();
        Vector3d v = random.randFisherVector(muvec, kappa);

	v = v.getUnitVector();
	candidate.source.setDirection(v);
	candidate.created.setDirection(v);
	candidate.previous.setDirection(v);
	candidate.current.setDirection(v);

	//set the weight of the particle, see eq. 3.1 of PoS(ICRC2019)447
	double pdfVonMises = kappa / (2. * M_PI * (1. - exp(-2. * kappa))) * exp(-kappa * (1. - v.dot(mu)));
	double weight = 1. / (4. * M_PI * pdfVonMises);
	candidate.setWeight(weight);
}

void SourceDirectedEmission::setDescription() {
	std::stringstream ss;
	ss << "SourceDirectedEmission: Random directed emission following the von-Mises-Fisher distribution with mean direction ";
	ss << mu << " and concentration parameter kappa = ";
	ss << kappa << "\n";
	description = ss.str();
}

// ----------------------------------------------------------------------------
SourceLambertDistributionOnSphere::SourceLambertDistributionOnSphere(const Vector3d &center, double radius, bool inward) :
		center(center), radius(radius) {
	this->inward = inward;
	setDescription();
}

void SourceLambertDistributionOnSphere::prepareParticle(ParticleState& particle) const {
	Random &random = Random::instance();
	Vector3d normalVector = random.randVector();
	particle.setPosition(center + normalVector * radius);
	double sign = inward ? -1 : 1; // negative (positive) Lamberts vector for inward (outward) directed emission
	particle.setDirection(Vector3d(0, 0, 0) + sign * random.randVectorLamberts(normalVector));
}

void SourceLambertDistributionOnSphere::setDescription() {
	std::stringstream ss;
	ss << "SourceLambertDistributionOnSphere: Random position and direction on a Sphere with center ";
	ss << center / kpc << " kpc and ";
	ss << radius / kpc << " kpc radius\n";
	description = ss.str();
}

// ----------------------------------------------------------------------------
SourceDirection::SourceDirection(Vector3d direction) :
		direction(direction) {
	setDescription();
}

void SourceDirection::prepareParticle(ParticleState& particle) const {
	particle.setDirection(direction);
}

void SourceDirection::setDescription() {
	std::stringstream ss;
	ss <<  "SourceDirection: Emission direction = " << direction << "\n";
	description = ss.str();
}

// ----------------------------------------------------------------------------
SourceEmissionMap::SourceEmissionMap(EmissionMap *emissionMap) : emissionMap(emissionMap) {
	setDescription();
}

void SourceEmissionMap::prepareCandidate(Candidate &candidate) const {
	if (emissionMap) {
		bool accept = emissionMap->checkDirection(candidate.source);
		candidate.setActive(accept);
	}
}

void SourceEmissionMap::setDescription() {
	description = "SourceEmissionMap: accept only directions from emission map\n";
}

void SourceEmissionMap::setEmissionMap(EmissionMap *emissionMap) {
	this->emissionMap = emissionMap;
}

// ----------------------------------------------------------------------------
SourceEmissionCone::SourceEmissionCone(Vector3d direction, double aperture) :
	aperture(aperture) {
	setDirection(direction);
	setDescription();
	
}

void SourceEmissionCone::prepareParticle(ParticleState& particle) const {
	Random &random = Random::instance();
	particle.setDirection(random.randConeVector(direction, aperture));
}

void SourceEmissionCone::setDirection(Vector3d dir) {
	if (dir.getR() == 0) {
		throw std::runtime_error("SourceEmissionCone: The direction vector was a null vector.");
	} else {
		direction = dir.getUnitVector();
	}
}

void SourceEmissionCone::setDescription() {
	std::stringstream ss;
	ss << "SourceEmissionCone: Jetted emission in ";
	ss << "direction = " << direction << " with ";
	ss << "half-opening angle = " << aperture << " rad\n";
	description = ss.str();
}

// ----------------------------------------------------------------------------
SourceRedshift::SourceRedshift(double z) :
		z(z) {
	setDescription();
}

void SourceRedshift::prepareCandidate(Candidate& candidate) const {
	candidate.setRedshift(z);
}

void SourceRedshift::setDescription() {
	std::stringstream ss;
	ss << "SourceRedshift: Redshift z = " << z << "\n";
	description = ss.str();
}

// ----------------------------------------------------------------------------
SourceUniformRedshift::SourceUniformRedshift(double zmin, double zmax) :
		zmin(zmin), zmax(zmax) {
	setDescription();
}

void SourceUniformRedshift::prepareCandidate(Candidate& candidate) const {
	double z = Random::instance().randUniform(zmin, zmax);
	candidate.setRedshift(z);
}

void SourceUniformRedshift::setDescription() {
	std::stringstream ss;
	ss << "SourceUniformRedshift: Uniform redshift in z = ";
	ss << zmin << " - " << zmax << "\n";
	description = ss.str();
}

// ----------------------------------------------------------------------------
SourceRedshiftEvolution::SourceRedshiftEvolution(double m, double zmin, double zmax) : m(m), zmin(zmin), zmax(zmax) {
	std::stringstream ss;
	ss << "SourceRedshiftEvolution: (1+z)^m, m = " << m;
	ss << ", z = " << zmin << " - " << zmax << "\n";
	description = ss.str();
}

void SourceRedshiftEvolution::prepareCandidate(Candidate& candidate) const {
	double x = Random::instance().randUniform(0, 1);
	double norm, z;

	// special case: m=-1
	if ((std::abs(m+1)) < std::numeric_limits<double>::epsilon()) {
		norm = log1p(zmax) - log1p(zmin);
		z = exp(norm*x) * (1+zmin) - 1;
	} else {
		norm = ( pow(1+zmax, m+1) - pow(1+zmin, m+1) ) / (m+1);
		z = pow( norm*(m+1)*x + pow(1+zmin, m+1), 1./(m+1)) - 1;
	}
	candidate.setRedshift(z);
}

// ----------------------------------------------------------------------------
SourceRedshift1D::SourceRedshift1D() {
	setDescription();
}

void SourceRedshift1D::prepareCandidate(Candidate& candidate) const {
	double d = candidate.source.getPosition().getR();
	double z = comovingDistance2Redshift(d);
	candidate.setRedshift(z);
}

void SourceRedshift1D::setDescription() {
	description = "SourceRedshift1D: Redshift according to source distance\n";
}

// ----------------------------------------------------------------------------
#ifdef CRPROPA_HAVE_MUPARSER
SourceGenericComposition::SourceGenericComposition(double Emin, double Emax, std::string expression, size_t bins) :
	Emin(Emin), Emax(Emax), expression(expression), bins(bins) {

	// precalculate energy bins
	double logEmin = ::log10(Emin);
	double logEmax = ::log10(Emax);
	double logStep = (logEmax - logEmin) / bins;
	energy.resize(bins + 1);
	for (size_t i = 0; i <= bins; i++) {
		energy[i] = ::pow(10, logEmin + i * logStep);
	}
	setDescription();
}

void SourceGenericComposition::add(int id, double weight) {
	int A = massNumber(id);
	int Z = chargeNumber(id);

	Nucleus n;
	n.id = id;

	// calculate nuclei cdf
	mu::Parser p;
	double E;
	p.DefineVar("E", &E);
	p.DefineConst("Emin", Emin);
	p.DefineConst("Emax", Emax);
	p.DefineConst("bins", bins);
	p.DefineConst("A", (double)A);
	p.DefineConst("Z", (double)Z);

	p.DefineConst("MeV", MeV);
	p.DefineConst("GeV", GeV);
	p.DefineConst("TeV", TeV);
	p.DefineConst("PeV", PeV);
	p.DefineConst("EeV", EeV);

	p.SetExpr(expression);

	// calculate pdf
	n.cdf.resize(bins + 1);

	for (std::size_t i=0; i<=bins; ++i) {
		E = energy[i];
		n.cdf[i] = p.Eval();
	}

	// integrate
	for (std::size_t i=bins; i>0; --i) {
		n.cdf[i] = (n.cdf[i-1] + n.cdf[i]) * (energy[i] - energy[i-1]) / 2;
	}
	n.cdf[0] = 0;

	// cumulate
	for (std::size_t i=1; i<=bins; ++i) {
		n.cdf[i] += n.cdf[i-1];
	}

	nuclei.push_back(n);

	// update composition cdf
	if (cdf.size() == 0)
		cdf.push_back(weight * n.cdf.back());
	else
		cdf.push_back(cdf.back() + weight * n.cdf.back());
}

void SourceGenericComposition::add(int A, int Z, double a) {
	add(nucleusId(A, Z), a);
}

void SourceGenericComposition::prepareParticle(ParticleState& particle) const {
	if (nuclei.size() == 0)
		throw std::runtime_error("SourceComposition: No source isotope set");

	Random &random = Random::instance();


	// draw random particle type
	size_t iN = random.randBin(cdf);
	const Nucleus &n = nuclei.at(iN);
	particle.setId(n.id);

	// random energy
	double E = interpolate(random.rand() * n.cdf.back(), n.cdf, energy);
	particle.setEnergy(E);
}

void SourceGenericComposition::setDescription() {
	description = "Generice source composition" + expression;
}

#endif

// ----------------------------------------------------------------------------

SourceTag::SourceTag(std::string tag) {
	setTag(tag);
}

void SourceTag::prepareCandidate(Candidate &cand) const {
	cand.setTagOrigin(sourceTag);
}

void SourceTag::setDescription() {
	description = "SourceTag: " + sourceTag;
}

void SourceTag::setTag(std::string tag) {
	sourceTag = tag;
	setDescription();
}

// ----------------------------------------------------------------------------

SourceMassDistribution::SourceMassDistribution(ref_ptr<Density> density, double max, double x, double y, double z) : 
	density(density), maxDensity(max), xMin(-x), xMax(x), yMin(-y), yMax(y), zMin(-z), zMax(z) {}

void SourceMassDistribution::setMaximalDensity(double maxDensity) {
	if (maxDensity <= 0) {
		KISS_LOG_WARNING << "SourceMassDistribution: maximal density must be larger than 0. Nothing changed.\n";
		return;
	}
	this->maxDensity = maxDensity;
}

void SourceMassDistribution::setXrange(double xMin, double xMax) {
	if (xMin > xMax) {
		KISS_LOG_WARNING << "SourceMassDistribution: minimal x-value must not exceed the maximal one\n";
		return;
	}
	this -> xMin = xMin;
	this -> xMax = xMax;
}

void SourceMassDistribution::setYrange(double yMin, double yMax) {
	if (yMin > yMax) {
		KISS_LOG_WARNING << "SourceMassDistribution: minimal y-value must not exceed the maximal one\n";
		return;
	}
	this -> yMin = yMin;
	this -> yMax = yMax;
}

void SourceMassDistribution::setZrange(double zMin, double zMax) {
	if (zMin > zMax) {
		KISS_LOG_WARNING << "SourceMassDistribution: minimal z-value must not exceed the maximal one\n";
		return;
	}
	this -> zMin = zMin;
	this -> zMax = zMax;
}

Vector3d SourceMassDistribution::samplePosition() const {
	Vector3d pos; 
	Random &rand = Random::instance();

	for (int i = 0; i < maxTries; i++) {
		pos.x = rand.randUniform(xMin, xMax);
		pos.y = rand.randUniform(yMin, yMax);
		pos.z = rand.randUniform(zMin, zMax);

		double n_density = density->getDensity(pos) / maxDensity;
		double n_test = rand.rand();
		if (n_test < n_density) {
			return pos;
		}
	}
	KISS_LOG_WARNING << "SourceMassDistribution: sampling a position was not possible within " 
		<< maxTries << " tries. Please check the maximum density or increse the number of maximal tries. \n";
	return Vector3d(0.);
}	

void SourceMassDistribution::prepareParticle(ParticleState &state) const {
	Vector3d pos = samplePosition();
	state.setPosition(pos);
}

void SourceMassDistribution::setMaximalTries(int tries) {
	this -> maxTries = tries;
}

std::string SourceMassDistribution::getDescription() {
	std::stringstream ss;
	ss << "SourceMassDistribuion: following the density distribution :\n";
	ss << "\t" << density -> getDescription();
	ss << "with a maximal density of " << maxDensity << " / m^3 \n";
	ss << "using the sampling range: \n";
	ss << "\t x in [" << xMin / kpc << " ; " << xMax / kpc << "] kpc \n";
	ss << "\t y in [" << yMin / kpc << " ; " << yMax / kpc << "] kpc \n";
	ss << "\t z in [" << zMin / kpc << " ; " << zMax / kpc << "] kpc \n";
	ss << "with maximal number of tries for sampling of " << maxTries << "\n";

	return ss.str();
}


} // namespace crpropa
