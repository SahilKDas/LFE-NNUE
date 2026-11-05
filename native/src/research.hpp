#pragma once
#include "simulation.hpp"
#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
namespace life {
class ResearchSuite {
public:
 struct Limits {size_t ram=128u*1024u*1024u;uint64_t disk=2ULL*1024*1024*1024;int interval=600;};
private:
 struct Stored {CheckpointInfo info;std::vector<uint64_t> genomes;bool pinned{};};
 std::filesystem::path root_,cache_,blobs_,library_;Limits limits_;
 std::mutex mutex_,jobsMutex_;std::condition_variable wake_,idle_;std::deque<std::function<void()>> jobs_;std::jthread worker_;
 std::atomic_bool stop_{false},working_{false},busy_{false},capturing_{false},cancel_{false};std::atomic<double> progress_{0};
 std::atomic<std::shared_ptr<const ResearchSnapshot>> published_;
 std::vector<Stored> stored_;std::vector<BranchSummary> branches_;std::vector<SimulationCommand> commands_;std::vector<JournalEntry> journal_;std::vector<LibraryItem> items_;
 std::shared_ptr<const GenomeRecord> left_,right_;GenomeComparison comparison_;std::shared_ptr<NativeSimulation> replacement_;
 uint64_t lastCommand_{},memoryBytes_{},diskBytes_{};
 uint64_t run_{},active_{1},nextBranch_{2},nextCheckpoint_{1},revision_{},epoch_{};int width_{},height_{},tick_{},presentTick_{};bool preview_{};double captureMs_{};ResearchEcology ecology_;std::string status_="Research ready";
 std::filesystem::path present_;std::vector<uint64_t> presentGenomes_;
 void publish();void status(std::string);void scanLibrary();
 Stored persist(const std::shared_ptr<NativeSimulation>&,uint64_t branch,bool pinned);
 void prune(uint64_t required,const std::vector<uint64_t>& protectedGenomes);
 uint64_t diskBytes()const;void garbageCollect();void removeCacheFile(const std::filesystem::path&);
 std::shared_ptr<NativeSimulation> restore(const Stored&);
 void requestReplacement(std::shared_ptr<NativeSimulation>,uint64_t branch,bool preview);
 void journal(JournalKind,const std::string&,const std::string& evidence="");
public:
 explicit ResearchSuite(std::filesystem::path root);
 ResearchSuite(std::filesystem::path root,Limits limits);
 ~ResearchSuite();
 ResearchSuite(const ResearchSuite&)=delete;
 void enqueue(std::function<void()>);
 void waitIdle();
 void attach(NativeSimulation&);
 bool observe(NativeSimulation&);
 bool stopsSimulation()const{return busy_.load()||preview();}
 bool preview()const{return published_.load()->preview;}
 bool busy()const{return busy_.load();}
 void cancel(){cancel_=true;}
 std::shared_ptr<const ResearchSnapshot> snapshot()const{return published_.load();}
 void checkpoint(NativeSimulation&,bool pinned=false);
 void seek(NativeSimulation&,int tick);
 void returnToPresent();
 void branch(NativeSimulation&,const std::string& name);
 void activate(NativeSimulation&,uint64_t branch);
 void experiment(NativeSimulation&,int ticks,SimulationCommand intervention);
 void compare(NativeSimulation&,int left,int right);
 void compareLibrary(const std::filesystem::path&);
 void saveSpecimen(NativeSimulation&,int id,const std::string& name,const std::string& notes="");
 void saveWorld(NativeSimulation&,const std::string& name);
 void importLibrary(const std::filesystem::path&);
 void loadWorld(NativeSimulation&,const std::filesystem::path&);
 void introduce(NativeSimulation&,const std::filesystem::path&,int x,int y);
 void searchJournal(NativeSimulation&,const std::string&,int kind=-1);
};
}

