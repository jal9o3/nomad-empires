#include <iostream>
#include <string>
#include <fstream>
using namespace std;

class Screen {
    public:
        void show(string sourcefile) {
            // Loads from a text file and displays the contents to the stream
            string text;
            ifstream ifs(sourcefile);

            text.assign((istreambuf_iterator<char>(ifs)),
                        (istreambuf_iterator<char>()));
            
            cout << text;
        }

};


int main() {
    int selection;
    Screen currentScreen;
    
    cout << "NOMAD EMPIRES" << endl;
    cout << "1 - Start" << endl;
    cout << "2 - Settings" << endl;
    cout << "3 - Quit" << endl;

    cout << "Select an option: ";
    cin >> selection;

    switch (selection) {
        case 1:
            Screen().show("PlayScreen.txt");
            break;
        case 2:
            cout << "Opening settings..." << endl;
            break;
        case 3:
            cout << "Quitting..." << endl;
            break;
        default:
            cout << "Invalid option" << endl;
    }

    return 0;
}