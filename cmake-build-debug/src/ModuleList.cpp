#include "crpropa/ModuleList.h"
#include "crpropa/ProgressBar.h"

#if _OPENMP
#include <omp.h>
#define OMP_SCHEDULE static,100
#endif

#include <algorithm>
#include <csignal>
#ifndef sighandler_t
typedef void (*sighandler_t)(int);
#endif

using namespace std;

namespace crpropa {

int g_cancel_signal_flag = 0;

void g_cancel_signal_callback(int sig) {
	std::cerr << "crpropa::ModuleList: Signal " << sig << " (SIGINT/SIGTERM) received" << std::endl;
	g_cancel_signal_flag = sig;
}

ModuleList::ModuleList() : showProgress(false) {
}

ModuleList::~ModuleList() {
}

void ModuleList::setShowProgress(bool show) {
	showProgress = show;
}

void ModuleList::add(Module *module) {
	modules.push_back(module);
}

void ModuleList::remove(std::size_t i) {
	iterator module_i = modules.begin();
	std::advance(module_i, i);
	modules.erase(module_i);
}

std::size_t ModuleList::size() const {
        return modules.size();
}

ref_ptr<Module> ModuleList::operator[](const std::size_t i) {
	iterator module_i = modules.begin();
	std::advance(module_i, i);
	return *module_i;
}


void ModuleList::process(Candidate* candidate) const {
	module_list_t::const_iterator m;
	for (m = modules.begin(); m != modules.end(); m++)
		(*m)->process(candidate);
}

void ModuleList::process(ref_ptr<Candidate> candidate) const {
	process((Candidate*) candidate);
}

void ModuleList::run(Candidate* candidate, bool recursive, bool secondariesFirst) {
	// propagate primary candidate until finished
	while (candidate->isActive() && (g_cancel_signal_flag == 0)) {
		process(candidate);

		// propagate all secondaries before next step of primary
		if (recursive and secondariesFirst) {
			for (size_t i = 0; i < candidate->secondaries.size(); i++) {
				if (g_cancel_signal_flag != 0)
					break;
				run(candidate->secondaries[i], recursive, secondariesFirst);
			}
		}
	}

	// propagate secondaries after completing primary
	if (recursive and not secondariesFirst) {
		for (size_t i = 0; i < candidate->secondaries.size(); i++) {
			if (g_cancel_signal_flag != 0)
				break;
			run(candidate->secondaries[i], recursive, secondariesFirst);
		}
	}
}

void ModuleList::run(ref_ptr<Candidate> candidate, bool recursive, bool secondariesFirst) {
	run((Candidate*) candidate, recursive, secondariesFirst);
}

void ModuleList::run(const candidate_vector_t *candidates, bool recursive, bool secondariesFirst) {
	size_t count = candidates->size();

#if _OPENMP
	std::cout << "crpropa::ModuleList: Number of Threads: " << omp_get_max_threads() << std::endl;
#endif

	ProgressBar progressbar(count);

	if (showProgress) {
		progressbar.start("Run ModuleList");
	}

	g_cancel_signal_flag = 0;
	sighandler_t old_sigint_handler = ::signal(SIGINT,
			g_cancel_signal_callback);
	sighandler_t old_sigterm_handler = ::signal(SIGTERM,
			g_cancel_signal_callback);

#pragma omp parallel for schedule(OMP_SCHEDULE)
	for (size_t i = 0; i < count; i++) {
		if (g_cancel_signal_flag != 0)
			continue;

		try {
			run(candidates->operator[](i), recursive);
		} catch (std::exception &e) {
			std::cerr << "Exception in crpropa::ModuleList::run: " << std::endl;
			std::cerr << e.what() << std::endl;
		}

		if (showProgress)
#pragma omp critical(progressbarUpdate)
			progressbar.update();
	}

	::signal(SIGINT, old_sigint_handler);
	::signal(SIGTERM, old_sigterm_handler);
	// Propagate signal to old handler.
	if (g_cancel_signal_flag > 0)
		raise(g_cancel_signal_flag);
}

void ModuleList::run(SourceInterface *source, size_t count, bool recursive, bool secondariesFirst) {

#if _OPENMP
	std::cout << "crpropa::ModuleList: Number of Threads: " << omp_get_max_threads() << std::endl;
#endif

	ProgressBar progressbar(count);

	if (showProgress) {
		progressbar.start("Run ModuleList");
	}

	g_cancel_signal_flag = 0;
	sighandler_t old_signal_handler = ::signal(SIGINT,
			g_cancel_signal_callback);
	sighandler_t old_sigterm_handler = ::signal(SIGTERM,
			g_cancel_signal_callback);

#pragma omp parallel for schedule(OMP_SCHEDULE)
	for (size_t i = 0; i < count; i++) {
		if (g_cancel_signal_flag !=0)
			continue;

		ref_ptr<Candidate> candidate;

		try {
			candidate = source->getCandidate();
		} catch (std::exception &e) {
			std::cerr << "Exception in crpropa::ModuleList::run: source->getCandidate" << std::endl;
			std::cerr << e.what() << std::endl;
#pragma omp critical(g_cancel_signal_flag)
			g_cancel_signal_flag = -1;
		}

		if (candidate.valid()) {
			try {
				run(candidate, recursive);
			} catch (std::exception &e) {
				std::cerr << "Exception in crpropa::ModuleList::run: " << std::endl;
				std::cerr << e.what() << std::endl;
#pragma omp critical(g_cancel_signal_flag)
				g_cancel_signal_flag = -1;
			}
		}

		if (showProgress)
#pragma omp critical(progressbarUpdate)
			progressbar.update();
	}

	::signal(SIGINT, old_signal_handler);
	::signal(SIGTERM, old_sigterm_handler);
	// Propagate signal to old handler.
	if (g_cancel_signal_flag > 0)
		raise(g_cancel_signal_flag);
}

ModuleList::iterator ModuleList::begin() {
	return modules.begin();
}

ModuleList::const_iterator ModuleList::begin() const {
	return modules.begin();
}

ModuleList::iterator ModuleList::end() {
	return modules.end();
}

ModuleList::const_iterator ModuleList::end() const {
	return modules.end();
}

std::string ModuleList::getDescription() const {
	std::stringstream ss;
	ss << "ModuleList\n";
	crpropa::ModuleList::module_list_t::const_iterator m;
	for (m = modules.begin(); m != modules.end(); m++)
		ss << "  " << (*m)->getDescription() << "\n";
	return ss.str();
}

void ModuleList::showModules() const {
	std::cout << getDescription();
}

ModuleListRunner::ModuleListRunner(ModuleList *mlist) : mlist(mlist) {
}

void ModuleListRunner::process(Candidate *candidate) const {
	if (mlist.valid())
		mlist->run(candidate);
}

std::string ModuleListRunner::getDescription() const {
	std::stringstream ss;
	ss << "ModuleListRunner\n";
	if (mlist.valid())
		ss << mlist->getDescription();
	return ss.str();
};

} // namespace crpropa
