#include <stdio.h>
#include <windows.h>
#include <iostream>
#include <string>
#include <vector>
#include <bitset>
#include <set>
#include <algorithm>
#include <random>
#include <iterator>
#include <numeric>
#include <chrono>
#include <iomanip> // Include header for std::setprecision and std::fixed
#include "libxl.h"

using namespace libxl;

// Define a structure for the result that contains an integer and a double, which allows returning multiple results (the minimum fault injection rounds in a single experiment, and the average fault nibble required to recover 64 S-box values)
struct Result {
    int returnFaultRound;
    double returnFaultNibble;
};

int Ascon[32] = { 4,11,31,20,26,21,9,2,27,5,8,18,29,3,6,28,30,19,7,14,0,13,17,24,16,12,1,25,22,10,15,23 };     // Ascon 5-bit S-box

int Sbox[64] = { 0 };     // Store the correct input values for Ascon's finalization S-box in the last round
int f_Sbox[64] = { 0 };   // Store the incorrect input values for Ascon's finalization S-box after injecting a fault (kicking off the 3rd bit)
int fault[64] = { 0 };    // Store the injected nibble fault values for Ascon's finalization S-box

// Function to set the S-box with random values
// This function simulates an S-box fault injection operation
// during the final round p-permutation phase in the Finalization stage.
// It first generates random S-box input values for testing fault injection.
void set_Sbox(int Sbox[]) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> distribution(0, 31);

    // Generate random numbers and take the modulo 32 to fill the array
    for (int i = 0; i < 64; ++i) {
        int randomNum = distribution(gen);
        Sbox[i] = randomNum;
    }
}

// Function to set the fault array with random values
void set_fault(int F[]) {
    std::random_device rd;
    std::mt19937 gen(rd());

    // Uniform fault injection
    std::uniform_int_distribution<int> distribution(0, 31);

    for (int i = 0; i < 64; ++i) {
        int randomNum = distribution(gen);
        F[i] = randomNum;
    }

    // Custom fault injection scenario (commented out)
    /*
    std::vector<int> customDistribution = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0,   // 0: 1/6
        1, 2, 4, 8, 16, 1, 2, 4, 8, 16,   // 1/6 * 1/5
        3, 5, 6, 9, 10, 12, 17, 18, 20, 24,   // 1/6 * 1/10
        7, 11, 13, 14, 19, 21, 22, 25, 26, 28, // 1/6 * 1/10
        15, 23, 27, 29, 30, 15, 23, 27, 29, 30, // 1/6 * 1/5
        31, 31, 31, 31, 31, 31, 31, 31, 31, 31  // 1/6
    };

    std::uniform_int_distribution<int> distribution(0, customDistribution.size() - 1);
    for (int i = 0; i < 64; ++i) {
        int randomIndex = distribution(gen);
        F[i] = customDistribution[randomIndex];
    }
    */
}

// Function to calculate the intersection of two sets
std::vector<int> calculateIntersection(const std::vector<int>& set1, const std::vector<int>& set2) {
    std::vector<int> intersection;

    // Use set_intersection to calculate the intersection
    std::set_intersection(
        set1.begin(), set1.end(),
        set2.begin(), set2.end(),
        std::back_inserter(intersection)
    );

    return intersection;
}

// Function to perform the Ascon fault injection trial
Result Ascon_trial(Sheet* sheet, int Num) {
    int out2;

    // Define a 3D dynamic array: 32x4x?
    std::vector<std::vector<std::vector<int>>> differ_LSB_2(32, std::vector<std::vector<int>>(4));

    // Solve the differential equation
    for (int i = 0; i < 32; i++) {
        for (int j = 0; j < 32; j++) {
            for (int in = 0; in < 32; in++) {
                out2 = Ascon[in] ^ Ascon[i & in];
                if (j == out2) {
                    differ_LSB_2[i][j % 4].push_back(in);
                }
            }
            std::sort(differ_LSB_2[i][j % 4].begin(), differ_LSB_2[i][j % 4].end()); // Sort the sets (important)
        }
    }

    // Set the S-box
    set_Sbox(Sbox);

    std::wstring S;
    for (int i = 0; i < 64; ++i) {
        S += std::to_wstring(Sbox[i]) + L",";
    }
    sheet->writeStr(Num, 1, S.c_str());

    // Perform second-stage fault injection
    const int COUNT = 100;
    int count;
    int temp = 0;

    // Define a 3D dynamic array: COUNT x 64 x ?, to store the set of possible S-box values after each fault injection
    std::vector<std::vector<std::vector<int>>> Intersection(COUNT, std::vector<std::vector<int>>(64));
    // Define a 2D dynamic array: 64 x ?, to store the intersection of two rows of S-box possible values
    std::vector<std::vector<int>> Intersec(64);

    std::wstring Count_and;
    std::wstring Count_all;

    // Define Countand array to store the random-and fault injection counts for each S-box possible value
    int Countand[64] = { 0 };

    for (count = 0; count < COUNT; count++) {
        // Set the fault
        set_fault(fault);

        // Print the injected fault values
        std::wstring f;
        for (int i = 0; i < 64; ++i) {
            f += std::to_wstring(fault[i]) + L",";
        }
        sheet->writeStr(Num, 5 + count * 3, f.c_str());

        // Inject faults into the S-box
        for (int i = 0; i < 64; ++i) {
            f_Sbox[i] = Sbox[i] & fault[i];
        }

        // Print the S-box output differences
        std::wstring dif;
        for (int i = 0; i < 64; ++i) {
            dif += std::to_wstring(Ascon[Sbox[i]] ^ Ascon[f_Sbox[i]]) + L",";
        }
        sheet->writeStr(Num, 6 + count * 3, dif.c_str());

        // Fill the array with intersections
        for (int i = 0; i < 64; ++i) {
            Intersection[count][i] = differ_LSB_2[fault[i]][(Ascon[Sbox[i]] ^ Ascon[f_Sbox[i]]) % 4];
        }

        // Initialize the intersection array on the first fault injection
        std::wstring Sb;
        if (count == 0) {
            for (int i = 0; i < 64; ++i) {
                Intersec[i] = Intersection[0][i];
                Sb += L"{";
                for (int j = 0; j < Intersec[i].size(); ++j) {
                    Sb += std::to_wstring(Intersec[i][j]) + L" ";
                }
                Sb += L"},";

                sheet->writeStr(Num, 7, Sb.c_str());
            }
        }

        // For subsequent fault injections, calculate the intersection with existing S-box possible values
        std::wstring Sbb;
        if (count > 0) {
            for (int i = 0; i < 64; ++i) {
                Intersec[i] = calculateIntersection(Intersection[count][i], Intersec[i]);
                Sbb += L"{";
                for (int j = 0; j < Intersec[i].size(); ++j) {
                    Sbb += std::to_wstring(Intersec[i][j]) + L" ";
                }
                Sbb += L"},";

                sheet->writeStr(Num, 7 + count * 3, Sbb.c_str());
                temp += Intersec[i].size();

                if ((Intersec[i].size() == 1) && (Countand[i] == 0)) { // When the first S-box possible value becomes unique, record the fault round
                    Countand[i] = count + 1;
                }
            }

            if (temp == 64) {
                break; // End experiment if all S-box values are uniquely determined
            }
            else {
                temp = 0;
            }
        }
    }

    // Calculate averages
    int sum2 = 0;
    for (int i = 0; i < 64; ++i) {
        Count_and += std::to_wstring(Countand[i]) + L",";
        sum2 += Countand[i];
    }

    double Average_Nibble2 = double(sum2) / 64;
    Count_and += L"\nAverage random-and faults for 64 S-boxes: " + std::to_wstring(Average_Nibble2);

    // Output the results to Excel
    sheet->writeStr(Num, 4, Count_and.c_str());

    Result result;
    result.returnFaultRound = count + 1;
    result.returnFaultNibble = Average_Nibble2;

    return result;
}

int main() {
    // Start timing
    auto start = std::chrono::high_resolution_clock::now();

    const int trial_Num = 10000; // Number of trials in each group of experiments
    int Count[trial_Num] = { 0 };
    double Countnibble[trial_Num] = { 0 };
    int temp = 0;
    double temp1 = 0;

    // Create an Excel document object
    libxl::Book* book = xlCreateBook();
    book->setKey(L"libxl", L"windows-28232b0208c4ee0369ba6e68abv6v5i3");
    if (book) {
        libxl::Sheet* sheet = book->addSheet(L"Sheet1");

        // Set table row titles
        sheet->writeStr(0, 1, L"5-bit S-box values");
        sheet->writeStr(0, 2, L"Total fault injection rounds to recover each S-box input value");
        sheet->writeStr(0, 3, L"Total random-xor fault injections for each S-box input value");
        sheet->writeStr(0, 4, L"Total random-and fault injections for each S-box input value");

        for (int i = 0; i < 100; ++i) { // Set a maximum of 100 fault injections per experiment group
            std::wstring i_str = std::to_wstring(i + 1);

            std::wstring output_str_1 = L"Experiment " + i_str + L" 5-bit fault values";
            std::wstring output_str_2 = L"Experiment " + i_str + L" S-box output differences";
            std::wstring output_str_3 = L"Experiment " + i_str + L" S-box possible input value sets";

            sheet->writeStr(0, 5 + 3 * i, output_str_1.c_str());
            sheet->writeStr(0, 6 + 3 * i, output_str_2.c_str());
            sheet->writeStr(0, 7 + 3 * i, output_str_3.c_str());
        }

        for (int i = 0; i < trial_Num; ++i) {
            std::wstring i_str = std::to_wstring(i + 1);
            std::wstring output_str = L"Experiment #" + i_str + L":";
            sheet->writeStr(i + 1, 0, output_str.c_str());

            Result result = Ascon_trial(sheet, i + 1);
            Count[i] = result.returnFaultRound;
            Countnibble[i] = result.returnFaultNibble;
            temp += Count[i];
            temp1 += Countnibble[i];
        }

        // End timing
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

        // Calculate average fault injection rounds and fault nibble
        double Average = double(temp) / double(trial_Num);
        double Averagenibble = double(temp1) / double(trial_Num);
        std::wstring newStr = L"Average fault injection rounds: " + std::to_wstring(Average) + L" Average fault nibble: " + std::to_wstring(Averagenibble) + L" Total time: " + std::to_wstring(duration.count() / 1'000'000.0) + L"s.";
        std::cout << Average << std::endl;
        std::cout << Averagenibble << std::endl;
        sheet->writeStr(0, 0, newStr.c_str());

        // Save the Excel file
        book->save(L"Ascon_random-and_trials.xlsx");

        // Release resources
        book->release();
        std::cout << "Excel file generated successfully!" << std::endl;
    }
    else {
        std::cerr << "Unable to create Excel document object" << std::endl;
    }

    system("pause");
    return 0;
}
