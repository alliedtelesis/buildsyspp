/******************************************************************************
 Copyright 2013 Allied Telesis Labs Ltd. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

    1. Redistributions of source code must retain the above copyright notice,
       this list of conditions and the following disclaimer.

    2. Redistributions in binary form must reproduce the above copyright
       notice, this list of conditions and the following disclaimer in the
       documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*******************************************************************************/

#include "include/buildsys.h"
#include "interface/luainterface.h"

#ifndef TAR_CMD
#define TAR_CMD "/bin/tar"
#endif

using std::chrono::duration_cast;
using std::chrono::steady_clock;

bool Package::quiet_packages = false;
bool Package::keep_staging = false;
bool Package::extract_in_parallel = true;
std::string Package::build_cache;
bool Package::clean_all_packages = false;
std::list<std::string> Package::overlays = {"."};
std::list<std::string> Package::forced_packages;

/**
 * Configure created packages to be 'quiet'.
 *
 * @param set - true to enable, false to disable.
 */
void Package::set_quiet_packages(bool set)
{
	quiet_packages = set;
}

/**
 * Configure created packages to keep their staging directories.
 *
 * @param set - true to enable, false to disable.
 */
void Package::set_keep_all_staging(bool set)
{
	keep_staging = set;
}

/**
 * Configure packages to extract in parallel.
 *
 * @param set - true to enable, false to disable.
 */
void Package::set_extract_in_parallel(bool set)
{
	extract_in_parallel = set;
}

/**
 *  Set the location of the build output cache
 *
 *  @param cache - The location to set.
 */
void Package::set_build_cache(std::string cache)
{
	build_cache = std::move(cache);
}

/**
 * Configure all packages to clean before building.
 *
 * @param set - true to enable, false to disable.
 */
void Package::set_clean_packages(bool set)
{
	clean_all_packages = set;
}

/**
 *  Add an overlay to search for package files
 *
 *  @param path - The overlay path to add.
 *  @param top - Make this more important than other overlays
 */
void Package::add_overlay_path(std::string path, bool top)
{
	if(top) {
		auto iter = overlays.begin();
		advance(iter, 1); // skip over the "." path
		overlays.insert(iter, std::move(path));
	} else {
		overlays.push_back(std::move(path));
	}
}

/**
 * Add a package to be 'forced' built. This will automatically
 * turn on forced mode building.
 *
 * @param name - The name of the package to force build.
 */
void Package::add_forced_package(std::string name)
{
	forced_packages.push_back(std::move(name));
}

/**
 * Get whether we operating in 'forced' mode. This means that we ignore
 * any detection of what needs building, and build only a specific set
 * of packages.
 *
 * @returns true if we are in forced mode, false otherwise.
 */
bool Package::is_forced_mode()
{
	return !forced_packages.empty();
}

void Package::common_init()
{
	std::string prefix = this->ns->getName() + "," + this->name;

	if(Package::quiet_packages) {
		auto log_path = boost::format{"%1%/output/%2%/%3%/build.log"} % this->pwd %
		                this->getNS()->getName() % this->name;
		this->logger = std::move(Logger(prefix, log_path.str()));
	} else {
		this->logger = std::move(Logger(prefix));
	}

	if(Package::clean_all_packages) {
		this->clean_before_build = true;
	}

	char *_pwd = getcwd(nullptr, 0);
	this->pwd = std::string(_pwd);
	free(_pwd); // NOLINT

	this->bd = BuildDir(this->pwd, this->getNS()->getName(), this->name);
}

Package::Package(NameSpace *_ns, std::string _name, std::string _file_short,
                 std::string _file)
    : name(std::move(_name)), file(std::move(_file)), file_short(std::move(_file_short)),
      ns(_ns)
{
	this->common_init();
}

Package::Package(NameSpace *_ns, std::string _name) : name(std::move(_name)), ns(_ns)
{
	std::string lua_file;
	std::string lastPart = this->name;

	size_t lastSlash = this->name.rfind('/');
	if(lastSlash != std::string::npos) {
		lastPart = this->name.substr(lastSlash + 1);
	}

	bool found = false;
	auto relative_fname = boost::format{"package/%1%/%2%.lua"} % this->name % lastPart;
	for(const auto &ov : Package::overlays) {
		lua_file = ov + "/" + relative_fname.str();
		if(filesystem::exists(lua_file)) {
			found = true;
			break;
		}
	}

	if(!found) {
		throw CustomException("Package not found: " + this->name);
	}

	this->file = lua_file;
	this->file_short = relative_fname.str();

	this->common_init();
}

BuildDir *Package::builddir()
{
	return &this->bd;
}

std::string Package::absolute_fetch_path(const std::string &location)
{
	return this->pwd + "/" + this->relative_fetch_path(location);
}

std::string Package::relative_fetch_path(const std::string &location, bool also_root)
{
	std::string src_path;

	if(location.at(0) == '/' || location.find("dl/") == 0) {
		src_path = location;
	} else {
		bool exists(false);

		if(location.at(0) == '.') {
			for(const auto &ov : Package::overlays) {
				src_path = ov + "/" + location;
				if(filesystem::exists(src_path)) {
					exists = true;
					break;
				}
			}
		} else {
			for(const auto &ov : Package::overlays) {
				src_path = ov + "/package/" + this->getName() + "/" + location;
				if(filesystem::exists(src_path)) {
					exists = true;
					break;
				}
				if(also_root) {
					src_path = ov + "/" + location;
					if(filesystem::exists(src_path)) {
						exists = true;
						break;
					}
				}
			}
		}
		if(!exists) {
			throw FileNotFoundException(location, this->getName());
		}
	}
	return src_path;
}

std::string Package::getFileHash(const std::string &filename)
{
	std::string hashes_file = this->relative_fetch_path("Digest");
	std::ifstream hashes(hashes_file);

	if(!hashes.is_open()) {
		return "";
	}

	std::string hash;
	std::string line;
	while(std::getline(hashes, line)) {
		auto split = line.find(' ');
		if(split != std::string::npos) {
			std::string fname = line.substr(0, split);
			if(fname == filename) {
				hash = line.substr(split + 1);
				break;
			}
		}
	}

	return hash;
}

void Package::printLabel(std::ostream &out)
{
	out << "[label=\"";

	out << this->getName() << "\\n";
	out << this->getNS()->getName() << "\\n";
	out << "Cmds:" << this->commands.size() << "\\n";
	out << "Time: " << this->run_secs << "s";

	out << "\"]";
}

bool Package::process()
{
	this->log(boost::format{"Processing (%1%)"} % this->file);

	this->build_description.add_package_file(this->file_short, hash_file(this->file));

	if(!interfaceSetup(&this->lua)) {
		this->log("interfaceSetup: Failed");
		exit(-1);
	}

	li_set_package(this);

	this->lua.processFile(this->file);

	return true;
}

/**
 * Extract the staging output for the package into the given directory.
 *
 * @param dir - The directory to extract the staging output into.
 *
 * @returns true if the extraction was successful, false otherwise.
 */
bool Package::extract_staging(const std::string &dir)
{
	PackageCmd pc(dir, TAR_CMD);
	pc.addArg("--no-same-owner");
	pc.addArg("-b");
	pc.addArg("256");
	pc.addArg("-xkf");
	std::string arg = this->pwd + "/output/" + this->getNS()->getName() + "/staging/" +
	                  this->name + ".tar";
	pc.addArg(arg);

	if(!pc.Run(&this->logger)) {
		this->log("Failed to extract staging_dir");
		return false;
	}

	return true;
}

/**
 * Extract the install output for the package into the given directory.
 *
 * @param dir - The directory to extract the install output into.
 *
 * @returns true if the extraction was successful, false otherwise.
 */
bool Package::extract_install(const std::string &dir)
{
	if(!this->installFiles.empty()) {
		for(const auto &install_file : this->installFiles) {
			PackageCmd pc(dir, "cp");
			std::string arg = this->pwd + "/output/" + this->getNS()->getName() +
			                  "/install/" + install_file;
			pc.addArg(arg);
			pc.addArg(install_file);

			if(!pc.Run(&this->logger)) {
				this->log(boost::format{"Failed to copy %1% (for install)"} % install_file);
				return false;
			}
		}
	} else {
		PackageCmd pc(dir, TAR_CMD);
		pc.addArg("--no-same-owner");
		pc.addArg("-b");
		pc.addArg("256");
		pc.addArg("-xkf");
		std::string arg = this->pwd + "/output/" + this->getNS()->getName() + "/install/" +
		                  this->name + ".tar";
		pc.addArg(arg);
		if(!pc.Run(&this->logger)) {
			this->log("Failed to extract install_dir");
			return false;
		}
	}

	return true;
}

bool Package::canBuild()
{
	if(this->isBuilt()) {
		return true;
	}

	for(auto &dp : this->depends) {
		if(!dp.getPackage()->isBuilt()) {
			return false;
		}
	}

	return true;
}

bool Package::ff_file(const std::string &hash, const std::string &rfile,
                      const std::string &path, const std::string &fname,
                      const std::string &fext)
{
	bool ret = false;
	std::string url = Package::build_cache + "/" + this->getNS()->getName() + "/" +
	                  this->getName() + "/" + hash + "/" + rfile;
	std::string cmd = "wget -q " + url + " -O " + path + "/" + fname + fext;
	int res = std::system(cmd.c_str());
	if(res != 0) {
		this->log("Failed to get " + rfile);
		ret = true;
	}
	return ret;
}

void Package::updateBuildInfoHashExisting()
{
	// populate the build.info hash
	std::string build_info_file = this->bd.getPath() + "/.build.info";
	this->buildinfo_hash = hash_file(build_info_file);
	this->log("Hash: " + this->buildinfo_hash);
}

void Package::updateBuildInfoHash()
{
	// populate the build.info hash
	std::string build_info_file = this->bd.getPath() + "/.build.info.new";
	this->buildinfo_hash = hash_file(build_info_file);
	this->log("Hash: " + this->buildinfo_hash);
}

Package::BuildInfoType Package::buildInfo(std::string *file_path, std::string *hash)
{
	if(this->isHashingOutput()) {
		*file_path = this->bd.getShortPath() + "/.output.info";
		*hash = hash_file(*file_path);
		return BuildInfoType::Output;
	}

	*file_path = this->bd.getShortPath() + "/.build.info";
	*hash = this->buildinfo_hash;
	return BuildInfoType::Build;
}

void Package::prepareBuildInfo()
{
	if(this->buildInfoPrepared) {
		return;
	}
	// Add the extraction info file
	std::string extraction_file_path;
	std::string extraction_file_hash;
	this->Extract.extractionInfo(&this->bd, &extraction_file_path, &extraction_file_hash);
	this->build_description.add_extraction_info_file(extraction_file_path,
	                                                 extraction_file_hash);

	// Add each of our dependencies build info files
	for(auto &dp : this->depends) {
		std::string file_path;
		std::string hash;
		auto type = dp.getPackage()->buildInfo(&file_path, &hash);

		if(hash.empty()) {
			this->log(boost::format{"build info for %1% is empty"} %
			          dp.getPackage()->getName());
			this->log("You probably need to build that package");
			exit(-1);
		}

		if(type == BuildInfoType::Output) {
			this->build_description.add_output_info_file(file_path, hash);
		} else {
			this->build_description.add_build_info_file(file_path, hash);
		}
	}

	// Create the new build info file
	std::string buildInfoFname = this->bd.getPath() + "/.build.info.new";
	std::ofstream _buildInfo(buildInfoFname.c_str());
	this->build_description.print(_buildInfo);
	_buildInfo.close();
	this->updateBuildInfoHash();
	this->buildInfoPrepared = true;
}

void Package::updateBuildInfo(bool updateOutputHash)
{
	// mv the build info file into the regular place
	std::string oldfname = this->bd.getPath() + "/.build.info.new";
	std::string newfname = this->bd.getPath() + "/.build.info";
	rename(oldfname.c_str(), newfname.c_str());

	if(updateOutputHash && this->isHashingOutput()) {
		// Hash the entire new path
		auto cmd = boost::format{"cd %1%; find -type f -exec sha256sum {} \\; | sort -k 2 "
		                         "> %2%/.output.info"} %
		           this->bd.getNewPath() % this->bd.getPath();
		std::system(cmd.str().c_str());
	}
}

bool Package::fetchFrom()
{
	bool ret = false;
	std::string staging_dir = this->getNS()->getStagingDir();
	std::string install_dir = this->getNS()->getInstallDir();
	std::vector<std::array<std::string, 4>> files = {
	    {"usable", staging_dir, this->name, ".tar.ff"},
	    {"staging.tar", staging_dir, this->name, ".tar"},
	    {"install.tar", install_dir, this->name, ".tar"},
	    {"output.info", this->bd.getPath(), ".output", ".info"},
	};

	this->log(boost::format{"FF URL: %1%/%2%/%3%/%4%"} % Package::build_cache %
	          this->getNS()->getName() % this->name % this->buildinfo_hash);

	if(!this->isHashingOutput()) {
		files.pop_back();
	}

	for(auto &element : files) {
		ret = this->ff_file(this->buildinfo_hash, element.at(0), element.at(1),
		                    element.at(2), element.at(3));
		if(ret) {
			break;
		}
	}

	if(ret) {
		this->log("Could not optimize away building");
	} else {
		this->log("Build cache used");

		this->updateBuildInfo(false);
	}

	return ret;
}

bool Package::shouldBuild()
{
	// we need to rebuild if the code is updated
	if(this->codeUpdated) {
		return true;
	}
	// we need to rebuild if installFiles is not empty
	if(!this->installFiles.empty()) {
		return true;
	}

	// lets make sure the install file (still) exists
	bool ret = false;

	std::string fname = this->pwd + "/output/" + this->getNS()->getName() + "/install/" +
	                    this->name + ".tar";

	if(!filesystem::exists(fname)) {
		ret = true;
	}

	// Now lets check that the staging file (still) exists
	fname = this->pwd + "/output/" + this->getNS()->getName() + "/staging/" + this->name +
	        ".tar";

	if(!filesystem::exists(fname)) {
		ret = true;
	}

	auto cmd =
	    boost::format{"cmp -s %1%/.build.info.new %1%/.build.info"} % this->bd.getPath();
	int res = std::system(cmd.str().c_str());

	// if there are changes,
	if(res != 0 || ret) {
		// see if we can grab new staging/install files
		if(!Package::build_cache.empty()) {
			ret = this->fetchFrom();
		} else {
			// otherwise, make sure we get (re)built
			ret = true;
		}
	}

	return ret;
}

/**
 * Get the set of packages that this package depends on.
 *
 * @param The set to fill with the depended packages.
 * @param include_children - Set true to include the child dependencies of any direct
 * dependencies.
 * @param ignore_intercept - Ignore the intercept setting on the depended package (i.e.
 * include its child dependencies).
 */
void Package::getDependedPackages(std::unordered_set<Package *> *packages,
                                  bool include_children, bool ignore_intercept)
{
	for(auto &dp : this->depends) {
		// This depended package (and therefore all of its dependencies) are already in the
		// set. Don't recurse through them again.
		if(packages->find(dp.getPackage()) != packages->end()) {
			continue;
		}

		packages->insert(dp.getPackage());

		if(include_children &&
		   (ignore_intercept || !dp.getPackage()->getInterceptInstall())) {
			dp.getPackage()->getDependedPackages(packages, include_children,
			                                     ignore_intercept);
		}
	}
}

/**
 * Get the set of packages required to populate the staging directory
 *
 * @param The set to fill with the staging packages.
 */
void Package::getStagingPackages(std::unordered_set<Package *> *packages)
{
	for(auto &dp : this->depends) {
		// This depended package is already in the set, don't add it again.
		if(packages->find(dp.getPackage()) != packages->end()) {
			continue;
		}

		packages->insert(dp.getPackage());

		// Recurse if this package doesn't intercept staging
		if(!dp.getPackage()->getInterceptStaging()) {
			dp.getPackage()->getStagingPackages(packages);
		}
	}
}

static void cleanDir(const std::string &dir)
{
	std::string cmd;

	cmd = "/bin/rm -fr " + dir;
	std::system(cmd.c_str());
	mkdir(dir.c_str(), 0777);
}

bool Package::prepareBuildDirs()
{
	this->log("Generating staging directory ...");

	// Clean out the (new) staging/install directories
	cleanDir(this->bd.getNewInstall());
	cleanDir(this->bd.getNewStaging());
	cleanDir(this->bd.getStaging());

	std::unordered_set<Package *> packages;
	this->getStagingPackages(&packages);

	std::list<std::thread> threads;
	std::atomic<bool> result{true};
	for(auto p : packages) {
		if(Package::extract_in_parallel) {
			std::thread th([p, this, &result] {
				bool ret = p->extract_staging(this->bd.getStaging());
				if(!ret) {
					result = false;
				}
			});
			threads.push_back(std::move(th));
		} else {
			result = p->extract_staging(this->bd.getStaging());
			if(!result) {
				break;
			}
		}
	}
	for(auto &t : threads) {
		t.join();
	}

	if(result) {
		this->log(boost::format{"Done (%1%)"} % packages.size());
	}

	return result;
}

bool Package::extractInstallDepends()
{
	if(this->depsExtraction.empty()) {
		return true;
	}

	// Extract installed files to a given location
	this->log("Removing old install files ...");
	PackageCmd pc(this->pwd, "/bin/rm");
	pc.addArg("-fr");
	pc.addArg(this->depsExtraction);
	if(!pc.Run(&this->logger)) {
		this->log(boost::format{"Failed to remove %1% (pre-install)"} %
		          this->depsExtraction);
		return false;
	}

	// Create the directory
	filesystem::create_directories(this->depsExtraction);

	this->log("Extracting installed files from dependencies ...");

	std::unordered_set<Package *> packages;
	this->getDependedPackages(&packages, !this->depsExtractionDirectOnly, false);

	std::list<std::thread> threads;
	std::atomic<bool> result{true};
	for(auto p : packages) {
		if(Package::extract_in_parallel) {
			std::thread th([p, this, &result] {
				bool ret = p->extract_install(this->depsExtraction);
				if(!ret) {
					result = false;
				}
			});
			threads.push_back(std::move(th));
		} else {
			result = p->extract_install(this->depsExtraction);
			if(!result) {
				break;
			}
		}
	}
	for(auto &t : threads) {
		t.join();
	}

	if(result) {
		this->log("Dependency install files extracted");
	}
	return result;
}

bool Package::packageNewStaging()
{
	PackageCmd pc(this->bd.getNewStaging(), TAR_CMD);
	pc.addArg("--numeric-owner");
	pc.addArg("-b");
	pc.addArg("256");
	pc.addArg("-cf");
	std::string arg = this->pwd + "/output/" + this->getNS()->getName() + "/staging/" +
	                  this->name + ".tar";
	pc.addArg(arg);
	pc.addArg(".");

	if(!pc.Run(&this->logger)) {
		this->log("Failed to compress staging directory");
		return false;
	}
	return true;
}

bool Package::packageNewInstall()
{
	if(!this->installFiles.empty()) {
		for(const auto &install_file : this->installFiles) {
			this->log("Copying " + install_file + " to output/" + this->getNS()->getName() +
			          "/install/");
			PackageCmd pc(this->bd.getNewInstall(), "cp");
			pc.addArg(install_file);
			std::string arg = this->pwd + "/output/" + this->getNS()->getName() +
			                  "/install/" + install_file;
			pc.addArg(arg);

			if(!pc.Run(&this->logger)) {
				this->log(boost::format{"Failed to copy install file (%1%)"} %
				          install_file);
				return false;
			}
		}
	} else {
		PackageCmd pc(this->bd.getNewInstall(), TAR_CMD);
		pc.addArg("--numeric-owner");
		pc.addArg("-b");
		pc.addArg("256");
		pc.addArg("-cf");
		std::string arg = this->pwd + "/output/" + this->getNS()->getName() + "/install/" +
		                  this->name + ".tar";
		pc.addArg(arg);
		pc.addArg(".");

		if(!pc.Run(&this->logger)) {
			this->log("Failed to compress install directory");
			return false;
		}
	}
	return true;
}

void Package::cleanStaging() const
{
	if(!Package::keep_staging && !this->suppress_remove_staging) {
		this->bd.cleanStaging();
	}
}

/**
 * Should we suppress building of this package.
 *
 * @returns true if building should be suppressed, false otherwise.
 */
bool Package::should_suppress_building()
{
	bool is_forced =
	    (std::find(Package::forced_packages.begin(), Package::forced_packages.end(),
	               this->name) != Package::forced_packages.end());
	return (this->is_forced_mode() && !is_forced);
}

bool Package::build(bool locally)
{
	// Hold the lock for the whole build, to avoid multiple running at once
	std::unique_lock<std::mutex> lk(this->lock);

	// Already build, pretend to successfully build
	if((locally && this->was_built) || (!locally && this->isBuilt())) {
		return true;
	}

	/* Check our dependencies are already built, or build them */
	for(auto &dp : this->depends) {
		if(!dp.getPackage()->build()) {
			return false;
		}
	}

	if(this->should_suppress_building()) {
		// Set the build.info hash based on what is currently present
		this->updateBuildInfoHashExisting();
		this->log("Building suppressed");
		// Just pretend we are built
		this->built = true;
		return true;
	}

	if(this->clean_before_build) {
		this->log("Cleaning");
		this->bd.clean();
	}

	// Create the new extraction.info file
	this->Extract.prepareNewExtractInfo(this, &this->bd);

	// Create the new build.info file
	this->prepareBuildInfo();

	// Check if building is required
	if(!locally && !this->shouldBuild()) {
		this->log("Not required");
		// Already built
		this->built = true;
		return true;
	}
	// Need to check that any packages that need to have been built locally
	// actually have been
	for(auto &dp : this->depends) {
		if(dp.getLocally()) {
			dp.getPackage()->log("Build triggered by " + this->getName());
			if(!dp.getPackage()->build(true)) {
				return false;
			}
		}
	}

	// Fetch anything we don't have yet
	if(!this->fetch()->fetch(&this->bd)) {
		this->log("Fetching failed");
		return false;
	}

	steady_clock::time_point start = steady_clock::now();

	if(this->Extract.extractionRequired(this, &this->bd)) {
		this->log("Extracting ...");
		if(!this->Extract.extract(this)) {
			return false;
		}
	}

	this->log("Building ...");
	// Clean new/{staging,install}, Extract the dependency staging directories
	if(!this->prepareBuildDirs()) {
		return false;
	}

	if(!this->extractInstallDepends()) {
		return false;
	}

	auto cIt = this->commands.begin();
	auto cEnd = this->commands.end();

	this->log("Running Commands");
	for(; cIt != cEnd; cIt++) {
		if(!(*cIt).Run(&this->logger)) {
			return false;
		}
	}
	this->log("Done Commands");

	this->log("BUILT");

	if(!this->packageNewStaging()) {
		return false;
	}

	if(!this->packageNewInstall()) {
		return false;
	}

	this->cleanStaging();

	this->updateBuildInfo();

	steady_clock::time_point end = steady_clock::now();

	this->run_secs = duration_cast<std::chrono::seconds>(end - start).count();
	this->log(boost::format{"Built in %1% seconds"} % this->run_secs);

	this->building = false;
	this->built = true;
	this->was_built = true;
	lk.unlock();

	return true;
}
