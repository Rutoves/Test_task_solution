#include <iostream>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/format.hpp>
#include <boost/regex.hpp>
#include <exception>
#include <string>
#include <algorithm>
#include <queue>
#include <set>

using std::cout;
using std::endl;
using namespace boost::filesystem;

#define TOTAL_BRICKS_CRITERION 0.1
#define MAX_MEMORY_CRITERION 4


struct analyze_res {
    bool res = false;
    double max_memory = 0;
    double total_bricks = 0;
};

void write_error(const path&p, const std::string& error) {
    ofstream report(p, std::ios::app);
    report << error << endl;
    report.close();
}

void clear_report(const path&p) {
    ofstream report(p);
    report << "";
    report.close();
}

std::string find_info_in_file(const std::string& str, const std::string& reg) {
    boost::regex reg_exp(reg);
    boost::smatch res;
    if(boost::regex_search(str, res, reg_exp)) {
        return res[1].str();
    }
    return "";
}

void find_max_memory(std::string& str, analyze_res& return_value) {
    std::string cur_max = find_info_in_file(str, R"(Memory Working Set Current = .* Mb, Memory Working Set Peak = (.*))");
    if (cur_max.empty())
        return;
    if (std::stod(cur_max) > return_value.max_memory)
            return_value.max_memory = std::stod(cur_max);
}

void find_total_bricks(std::string& str, analyze_res& return_value) {
    std::string cur_brick = find_info_in_file(str, R"(MESH::Bricks: Total=(.*) Gas=.* Solid=.* Partial=.* Irregular=.*)");
    if (cur_brick.empty())
        return;
    return_value.total_bricks = std::stod(cur_brick);
}

analyze_res file_analyze(const path& p, const std::string& file_path, const path& error_path) {
    ifstream file(p);
    std::string str;
    std::queue<std::string> errors;
    size_t str_number = 1;
    bool solver_flag = false;
    analyze_res return_value;
    while(std::getline(file, str)) {
        std::string str_copy = str;
        boost::algorithm::to_lower(str_copy);
        if (str_copy.find("error") != std::string::npos) {
            std::string temp_res;
            temp_res += file_path + "(";
            temp_res += std::to_string(str_number) + ")";
            temp_res += ": " + str;
            write_error(error_path, temp_res);
            return_value.res = true;
        }
        if (!solver_flag && str.find("Solver finished at") != std::string::npos)
            solver_flag = true;

        // find bricks and max memory
        find_max_memory(str, return_value);

        find_total_bricks(str, return_value);

        str_number++;
    }
    if (!solver_flag) {
        return_value.res = true;
        write_error(error_path, file_path + ": missing 'Solver finished at'");
    }
    file.close();
    return return_value;
}

std::string parse_path(const path& p) {
    size_t count = 0;
    path res;
    for (auto& element : p) {
        if (count > 1)
            res /= element;
        count++;
    }
    res /= "/";
    return res.string();
}

bool check_test_directories(path& test) {
    bool reference_flag = false;
    bool run_flag = false;

    clear_report(test / "/report.txt");
    for(directory_entry& file : directory_iterator(test)) {
        if (file.path().filename() == "ft_reference") {
            reference_flag = true;
        } else if (file.path().filename() == "ft_run") {
            run_flag = true;
        }
    }

    if (!(reference_flag && run_flag)) {
        std::string error_res = "directory missing: ";
        if (!reference_flag)
            error_res += "ft_reference";
        if (!run_flag)
            error_res += " ft_run";
        write_error(test / "/report.txt", error_res);
        return true;
    }
    return false;
}

void add_file_paths(const path& current_directory, std::set<path>& files) {
    std::queue<path> q;
    for (directory_entry& file_name : directory_iterator(current_directory))
        q.push(file_name.path());
    while(!q.empty()) {
        path current = q.front();
        q.pop();
        if (is_regular_file(current)) {
            auto temp = current.rbegin();
            path res = *temp;
            temp++;
            res = *temp / res;
            files.insert(res);
        } else {
            for (directory_entry &file_name : directory_iterator(current))
                q.push(file_name.path());
        }
    }
}

bool check_for_files_quantity(const std::set<path>& files, std::string message, const path& p) {
    bool res = false;
    if (!files.empty()) {
        res = true;
        for (auto& elem : files) {
            message += "'" + elem.string() + "'";
            if (elem != *files.rbegin())
                message += ", ";
        }
        write_error(p / "/report.txt", message);
    }
    return res;
}

class Test_checker {
public:

    static bool check_total_bricks(const analyze_res& reference_res, const analyze_res& run_res,
                            const path& elem, const path& test) {
        bool res = false;
        double rel_dif = std::max(reference_res.total_bricks, run_res.total_bricks)/
                  std::min(reference_res.total_bricks, run_res.total_bricks);
        if (rel_dif > TOTAL_BRICKS_CRITERION + 1) {
            res = true;
            boost::format brick_error =
                    boost::format("%s: different 'Total of bricks' "
                                  "(ft_run=%g,"
                                  " ft_reference=%g,"
                                  " rel.diff=%.2lf,"
                                  " criterion=%g)") % elem.string() % run_res.total_bricks %
                    reference_res.total_bricks % (rel_dif - 1) % TOTAL_BRICKS_CRITERION;
            write_error(test / "/report.txt", brick_error.str());
        }
        return res;
    }

    static bool check_max_memory(const analyze_res& reference_res, const analyze_res& run_res,
                          const path& elem, const path& test) {
        bool res = false;
        double rel_dif = std::max(reference_res.max_memory, run_res.max_memory)/
                         std::min(reference_res.max_memory, run_res.max_memory);
        if (rel_dif > MAX_MEMORY_CRITERION + 1) {
            res = true;
            boost::format memory_error =
                    boost::format("%s: different 'Memory Working Set Peak' "
                                  "(ft_run=%g,"
                                  " ft_reference=%g,"
                                  " rel.diff=%.2lf,"
                                  " criterion=%d)") % elem.string() % run_res.max_memory %
                    reference_res.max_memory % (rel_dif - 1) % MAX_MEMORY_CRITERION;
            write_error(test / "/report.txt", memory_error.str());
        }
        return res;
    }

};


#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc++17-extensions"
bool check_test_files(path& test) {
    std::set<path> reference_files;
    std::set<path> run_files;

    for(directory_entry& file : directory_iterator(test)) {
        if (file.path().filename() == "ft_reference") {
            add_file_paths(file, reference_files);
        } else if (file.path().filename() == "ft_run") {
            add_file_paths(file, run_files);
        }
    }

    std::set<path> extra_files;
    std::set<path> missing_files;

    std::set_difference(run_files.begin(), run_files.end(), reference_files.begin(), reference_files.end(),
                        std::inserter(extra_files, extra_files.begin()));
    std::set_difference(reference_files.begin(), reference_files.end(), run_files.begin(), run_files.end(),
                        std::inserter(missing_files, missing_files.begin()));
    bool files_res = false;
    files_res |= check_for_files_quantity(missing_files,
                                          "In ft_run there are missing files present in ft_reference: ",
                                            test);

    files_res |= check_for_files_quantity(extra_files,
                                          "In ft_run there are extra files not present in ft_reference: ",
                                          test);


    if (files_res)
        return true;

    bool res = false;
    for (const path& elem : reference_files) {
        analyze_res reference_res = file_analyze(
                test / "/ft_reference/" / elem,
                elem.string(),
                test / "/report.txt"
                );
        analyze_res run_res = file_analyze(
                test / "/ft_run/" / elem,
                elem.string(),
                test / "/report.txt"
        );
        res |= reference_res.res | run_res.res;

        res |= Test_checker::check_max_memory(reference_res, run_res, elem, test);

        res |= Test_checker::check_total_bricks(reference_res, run_res, elem, test);
    }
    if(res)
        return true;
    return false;
}
#pragma clang diagnostic pop

int main(int argc, char* argv[])
{

    path p ("../logs");

    try
    {
        if (exists(p))
        {
            if (is_directory(p))
            {
                std::vector<path> sorted_paths;
                for (directory_entry&  global_test : directory_iterator(p)) {
                    for (directory_entry &local_test : directory_iterator(global_test)) {
                        sorted_paths.push_back(local_test.path());
                    }
                }
                std::sort(sorted_paths.begin(), sorted_paths.end());
                for (auto& local_test : sorted_paths) {
                    bool res = false;
                    res = check_test_directories(local_test);
                    res |= check_test_files(local_test);
                    if(res) {
                           cout << "FAIL: " << parse_path(local_test) << endl;
                           // вывод report.txt
                           ifstream file(local_test / "report.txt");
                           std::string str;
                           while(std::getline(file, str)) {
                               cout << str << endl;
                           }
                    } else
                       cout << "OK: " << parse_path(local_test) << endl;
                }
            }
            else
                cout << p << " exists, but is not a directory\n";
        }
        else
            cout << p << " does not exist\n";
    }

    catch (const filesystem_error& ex)
    {
        cout << ex.what() << '\n';
    }
}