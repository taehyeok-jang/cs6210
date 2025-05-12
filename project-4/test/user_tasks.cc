/* DON'T MAKE ANY CHANGES IN THIS FILE */

#include <mr_task_factory.h>
#include <iostream>
#include <cstring>
#include <algorithm>
#include <numeric>

class UserMapper : public BaseMapper {

	public:
    virtual void map(const std::string& input_line) override {
      char * c_input = new char [input_line.length()+1];
      std::strcpy (c_input, input_line.c_str());
      static const char* delims = " ,.\"'";
      char *start, *save_pointer;
      start = strtok_r (c_input, delims, &save_pointer);
      while (start != NULL) {
        emit(start, "1");
        start = strtok_r (nullptr, delims, &save_pointer);
      }
      delete[] c_input;
    }
};


/**
 * e.g.
 * inputs: 
 * 	key = "hello"
 * 	values = ["1", "1", "1"]
 * 
 * returns:
 * 	key = "hello"
 * 	value = "3"
 * 
 * 1. convert values to ints
 * 2. sum the ints (accumulate)
 * 3. convert the sum to a string
 * 4. emit the key and value
 */
class UserReducer : public BaseReducer {

	public:
		virtual void reduce(const std::string& key, const std::vector<std::string>& values) override {
			std::vector<int> counts;
			std::transform(values.cbegin(), values.cend(), std::back_inserter(counts), [](const std::string numstr){ return atoi(numstr.c_str()); });
			emit(key, std::to_string(std::accumulate(counts.begin(), counts.end(), 0)));
		}

};


static std::function<std::shared_ptr<BaseMapper>() > my_mapper = 
		[] () { return std::shared_ptr<BaseMapper>(new UserMapper); };

static std::function<std::shared_ptr<BaseReducer>() > my_reducer = 
		[] () { return std::shared_ptr<BaseReducer>(new UserReducer); };


namespace {
	bool register_tasks_and_check() {
		
		const std::string user_id = "cs6210";
		if (!register_tasks(user_id, my_mapper, my_reducer)) {
			std::cout << "Failed to register user_id: " << user_id << std::endl;	
			exit (EXIT_FAILURE);
		}
		return true;
	}
}


bool just_store = register_tasks_and_check();
