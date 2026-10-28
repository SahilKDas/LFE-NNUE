#include "ui_commands.hpp"
#include <stdexcept>
#include <future>
#include <latch>
#include <thread>
#include <vector>
void expect(bool value){if(!value)throw std::runtime_error("UI command ordering/thread contract failed");}
int main(){
 life::UiCommands queue;std::vector<int> values;const auto ui=std::this_thread::get_id();std::latch ready(1),release(1);
 std::jthread coordinator([&]{ready.count_down();release.wait();queue.execute();});ready.wait();
 for(int i=0;i<100;++i)queue.submit([&,i]()->life::UiCommands::Reply{expect(std::this_thread::get_id()!=ui);return [&,i]{expect(std::this_thread::get_id()==ui);values.push_back(i);};});
 expect(values.empty()&&queue.pending());release.count_down();coordinator.join();expect(values.empty());queue.complete();expect(values.size()==100);for(int i=0;i<100;++i)expect(values[i]==i);queue.complete();expect(values.size()==100);
}
