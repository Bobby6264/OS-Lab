#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <bits/stdc++.h>

using namespace std;

void gendeep(int num, bool topnode)
{
    if (topnode)
    {
        ifstream infile("/root/OS/practice/foodep.txt");
        if (!infile)
        {
            cerr << "Unable to open foodep.txt" << endl;
            exit(1);
        }

        int n;
        infile >> n;
        infile.close();

        ofstream outfile("/root/OS/practice/done.txt");
        if (!outfile)
        {
            cerr << "Unable to create done.txt" << endl;
            exit(1);
        }
        outfile << "";
        for (int i = 0; i < n; ++i)
        {
            outfile << "0";
        }
        outfile.close();
    }

    ifstream infile("./foodep.txt");
    if (!infile)
    {
        cerr << "Unable to open foodep.txt" << endl;
        exit(1);
    }

    string line;
    int line_number = 0;
    while (getline(infile, line))
    {
        if (line_number == num)
        {
            // cout << "Line " << num << ": " << line << endl;
            vector<int> numbers;
            int n = line.size();
            int ind = 0;
            while (line[ind] != ':')
            {
                ind++;
            }
            ind++;
            string numstr = "";
            while (ind < n)
            {
                if (line[ind] == ' ' && numstr != "")
                {
                    if (!numstr.empty())
                    {
                        numbers.push_back(stoi(numstr));
                    }
                    numstr = "";
                }
                else
                {
                    numstr += line[ind];
                }
                ind++;
            }
            if (!numstr.empty())
            {
                numbers.push_back(stoi(numstr));
            }
            if (numbers.size() == 0)
            {
                cout << "foo"<<num<<" rebuilt" << endl;

                return;
            }
            for (std::vector<int>::size_type i = 0; i < numbers.size(); i++)
            {
                // cout<<numbers[i]<<endl;
                string new_line;
                ifstream infile1("./done.txt");
                if (!infile1)
                {
                    cerr << "Unable to open done.txt" << endl;
                    exit(1);
                }
                getline(infile1, new_line);
                infile1.close();
                if (new_line[numbers[i] - 1] == '0')
                {
                    new_line[numbers[i] - 1] = '1';
                    ofstream outfile("./done.txt");
                    if (!outfile)
                    {
                        cerr << "Unable to create done.txt" << endl;
                        exit(1);
                    }
                    outfile << new_line;
                    // cout<<new_line<<endl;
                    outfile.close();
                    // gendeep(numbers[i], false);
                    pid_t pid = fork();
                    if (pid == 0)
                    {
                        execl("./rebuild", "./rebuild", to_string(numbers[i]).c_str(), "dksgjhskjd", NULL);
                        exit(0);
                    }
                    else if (pid > 0)
                    {
                        wait(NULL);
                    }
                    else
                    {
                        cerr << "Fork failed" << endl;
                        exit(1);
                    }
                }
            }
            cout<<"foo"<<num<<" rebuilt from ";
            for(std::vector<int>::size_type i = 0; i < numbers.size(); i++){
                cout<<"foo"<<numbers[i]<<" ";
            }
            string linefinalupdate ;
            ifstream infile2("./done.txt");
            if (!infile2)
            {
                cerr << "Unable to open done.txt" << endl;
                exit(1);
            }
            getline(infile2, linefinalupdate);
            infile2.close();
            linefinalupdate[num-1] = '1';
            ofstream outfile2("./done.txt");
            if (!outfile2)
            {
                cerr << "Unable to create done.txt" << endl;
                exit(1);
            }
            outfile2 << linefinalupdate;
            outfile2.close();
            
            cout<<"\n";
            break;
        }
        line_number++;
    }

    infile.close();
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        printf("Usage: %s <integer>\n", argv[0]);
        return 1;
    }
    // cout<<argc<<endl;
    int num = atoi(argv[1]);
    bool topnode = (argc == 2);
    gendeep(num, topnode);

    return 0;
}