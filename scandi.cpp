#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <map>
#include <vector>
#include <cmath>
#include <algorithm>
#include <tuple>
#include <iomanip>

using namespace std;

// Function to split a string by a specific delimiter
vector<string> split(const string &line, char delimiter) {
    vector<string> tokens;
    stringstream ss(line);
    string token;
    while (getline(ss, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

// Function to compute the mean of a vector of doubles
double mean(const vector<double> &values) {
    if (values.empty()) return 0.0;
    double sum = 0.0;
    for (double v : values) sum += v;
    return sum / values.size();
}

// Function to compute the median of a vector of doubles
double median(vector<double> values) {
    if (values.empty()) return 0.0;
    nth_element(values.begin(), values.begin() + values.size() / 2, values.end());
    if (values.size() % 2 == 0) {
        nth_element(values.begin(), values.begin() + values.size() / 2 - 1, values.end());
        return (values[values.size() / 2 - 1] + values[values.size() / 2]) / 2.0;
    }
    return values[values.size() / 2];
}

// Function to compute the bid-ask spread
inline double computeBidAskSpread(double askPrice, double bidPrice) {
    return askPrice - bidPrice; 
}

// Function to check if the condition indicates an auction period
bool exclusionPeriods(const double &bidPrice, const double &askPrice, const int &tradeVolume, const string &conditionCode) {
    return (tradeVolume == 0 || (bidPrice > askPrice && conditionCode != "XT" && conditionCode != ""));
}

int main() {
    // Open input CSV file containing tick data
    ifstream infile("scandi/scandi.csv");

    // Map to store stock data (keyed by stock ID)
    map<string, map<string, vector<tuple<double, double, double, int, int, string, int>>>> stockData; // Map of stockId -> date -> data vector
    // Dummy item
    string line;

    // Read and process CSV data line by line
    while (getline(infile, line)) {
        auto tokens = split(line, ',');

        double bidPrice = stod(tokens[2]); // Adjusted for Bid Price column
        double askPrice = stod(tokens[3]); // Adjusted for Ask Price column
        int tradeVolume = stoi(tokens[7]); // Trade Volume column
        string conditionCode = tokens[14]; // Condition codes column

        if (exclusionPeriods(bidPrice, askPrice, tradeVolume, conditionCode)) {
            continue;
        }

        string stockId = tokens[0]; // Bloomberg Code/Stock identifier
        string date = tokens[10]; // Date column
        double tradePrice = stod(tokens[4]); // Adjusted for Trade Price column
        int updateType = stoi(tokens[8]); // Update type
        int time = stoi(tokens[11]); // Time in seconds past midnight

        // Store data keyed by stock ID and date
        stockData[stockId][date].emplace_back(bidPrice, askPrice, tradePrice, tradeVolume, time, conditionCode, updateType);
    }

    // Open output CSV file
    ofstream outfile("output.csv");

    // Write header to output file
    outfile << "Stock ID,Mean Time Between Trades,Median Time Between Trades,Mean Time Between Tick Changes,Median Time Between Tick Changes,"
            "Longest Time Between Trades,Longest Time Between Tick Changes,Mean Bid Ask Spread,Median Bid Ask Spread,"
            "Price Round Number Effect,Volume Round Number Effect\n";

    // Process each stock's data for analysis
    for (const auto &stockEntry : stockData) {
        const string &stockId = stockEntry.first;
        const auto &dateData = stockEntry.second;

        vector<double> tradeTimes, tickTimes, bidAskSpreads;
        double zeroAsPriceLastDigit_Count = 0.0, nonzeroAsPriceLastDigit_Count = 0.0;
        double zeroAsVolumeLastDigit_Count = 0.0, nonzeroAsVolumeLastDigit_Count = 0.0;

        // Process data by date
        for (const auto &dateEntry : dateData) {
            const string &date = dateEntry.first;
            vector<tuple<double, double, double, int, int, string, int>> data = dateEntry.second;

            // Sort the data by time (4th element in tuple)
            sort(data.begin(), data.end(), [](const tuple<double, double, double, int, int, string, int>& a, const tuple<double, double, double, int, int, string, int>& b) {
                return get<4>(a) < get<4>(b);
            });

            double lastTradeTime = -1;
            double lastTickChangeTime = -1;

            for (size_t i = 0; i < data.size(); ++i) {
                const auto &curr = data[i];
                int currUpdateType = get<6>(curr);
                double currTime = static_cast<double>(get<4>(curr)); // time in seconds past midnight

                // Time difference between trades (when updateType is 1)
                if (currUpdateType == 1) {
                    if (lastTradeTime >= 0) {
                        tradeTimes.push_back(currTime - lastTradeTime);
                    }
                    lastTradeTime = currTime;
                }

                // Time difference between tick changes (when updateType is 2 or 3)
                if (currUpdateType == 2 || currUpdateType == 3) {
                    if (lastTickChangeTime >= 0) {
                        tickTimes.push_back(currTime - lastTickChangeTime);
                    }
                    lastTickChangeTime = currTime;
                }

                // Calculate bid-ask spread
                bidAskSpreads.push_back(computeBidAskSpread(get<1>(curr), get<0>(curr)));

                // Accumulate round number events (last digit being zero)
                double currTradePrice = get<2>(curr);
                int currTradeVolume = get<3>(curr);

                // Check if round number
                if (static_cast<int>(currTradePrice) % 10 == 0) {
                    zeroAsPriceLastDigit_Count++;
                } else {
                    nonzeroAsPriceLastDigit_Count++;
                }

                if (currTradeVolume % 10 == 0) {
                    zeroAsVolumeLastDigit_Count++;
                } else {
                    nonzeroAsVolumeLastDigit_Count++;
                }
            }
        }

        // Calculate metrics across all days
        double meanTradeTime = mean(tradeTimes);
        double medianTradeTime = median(tradeTimes);
        double meanTickTime = mean(tickTimes);
        double medianTickTime = median(tickTimes);
        double longestTradeTime = tradeTimes.empty() ? 0.0 : *max_element(tradeTimes.begin(), tradeTimes.end());
        double longestTickTime = tickTimes.empty() ? 0.0 : *max_element(tickTimes.begin(), tickTimes.end());
        double meanBidAskSpread = mean(bidAskSpreads);
        double medianBidAskSpread = median(bidAskSpreads);

        double totalTradeCount = zeroAsPriceLastDigit_Count + nonzeroAsPriceLastDigit_Count;
        double priceRoundNumberEffect = totalTradeCount == 0 ? 0.0 : zeroAsPriceLastDigit_Count / totalTradeCount;
        double volumeRoundNumberEffect = totalTradeCount == 0 ? 0.0 : zeroAsVolumeLastDigit_Count / totalTradeCount;

        // Write to output file
        outfile << stockId << "," << meanTradeTime << "," << medianTradeTime << "," 
                << meanTickTime << "," << medianTickTime << "," << longestTradeTime << "," 
                << longestTickTime << "," << meanBidAskSpread << "," << medianBidAskSpread << "," 
                << priceRoundNumberEffect << "," << volumeRoundNumberEffect << "\n";
    }


    outfile.close();
    cout << "Processing complete. Output file closed.\n";
    return 0;

}
