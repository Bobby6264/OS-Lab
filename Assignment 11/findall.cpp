// Author: Karan Kumar Sethi
#include <bits/stdc++.h>
#include <filesystem>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

using namespace std;
namespace fs = std::filesystem;

#define endl "\n"
typedef long long ll;
#define rep(i, n) for (ll i = 0; i < n; i++)
#define repa(i, a, n) for (ll i = a; i < n; i++)
#define repab(i, a, n, b) for (ll i = a; i < n; i = i + b)
#define mod 1000000007
#define pb push_back
const ll N = 2e5 + 20, inf = 1e9;

struct uid_entry {
    uid_t uid;
    string login;
};

vector<uid_entry> uid_table;
int file_counter = 0;

void add_uid_entry(uid_t uid, const string &login) {
    uid_table.push_back({uid, login});
}

void load_uid_table() {
    // open passwd file to get user info
    ifstream passwd_file("/etc/passwd");
    if (!passwd_file.is_open()) {
        cerr << "Can't open /etc/passwd file: " << strerror(errno) << endl;
        exit(1);
    }

    string line;
    // read file line by line
    while (getline(passwd_file, line)) {
        // variables to store user info
        string username, dummy, uid_str;
        stringstream ss(line);
        
        // passwd file format is username:x:uid:...
        getline(ss, username, ':');
        getline(ss, dummy, ':');  // skip password field
        getline(ss, uid_str, ':');
        
        // make sure we got valid data
        if (username.size() > 0 && uid_str.size() > 0) {
            // convert string to uid number and add to table
            uid_t uid_num = stoi(uid_str);
            add_uid_entry(uid_num, username);
        }
    }
}

string uid_to_login(uid_t uid) {
    for (const auto &entry : uid_table) {
        if (entry.uid == uid) {
            return entry.login;
        }
    }
    return "UNKNOWN";
}

bool has_extension(const string &filename, const string &ext) {
    return filename.size() > ext.size() + 1 &&
           filename.substr(filename.size() - ext.size() - 1) == "." + ext;
}

void traverse(const string &path, const string &ext) {
    try {
        for (const auto &entry : fs::recursive_directory_iterator(path)) {
            if (entry.is_regular_file() && has_extension(entry.path().filename().string(), ext)) {
                struct stat statbuf;
                if (stat(entry.path().c_str(), &statbuf) == -1) {
                    cerr << "Error in stating: " << strerror(errno) << endl;
                    continue;
                }

                file_counter++;
                cout << file_counter << " : " << uid_to_login(statbuf.st_uid) << " "
                     << statbuf.st_size << " " << entry.path().string() << endl;
            }
        }
    } catch (const fs::filesystem_error &e) {
        cerr << "Error traversing directory: " << e.what() << endl;
    }
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        cerr << "Error: Incorrect usage. Provide directory name and extension." << endl;
        exit(1);
    }

    string dname = argv[1];
    string ext = argv[2];

    load_uid_table();

    cout << "NO : OWNER SIZE NAME" << endl;
    cout << "-- ----- ---- ----" << endl;

    traverse(dname, ext);

    cout << "+++ " << file_counter << " files match the extension " << ext << endl;

    return 0;
}