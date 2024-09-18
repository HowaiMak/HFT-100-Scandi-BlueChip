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
    map<string, vector<tuple<double, double, double, int, int, string, int>>> stockData;
    // Dummy item
    string line;

    // Read and process CSV data line by line
    while (getline(infile, line)) {
        auto tokens = split(line, ',');

        double bidPrice = stod(tokens[1]);
        double askPrice = stod(tokens[2]);
        int tradeVolume = stoi(tokens[7]);
        string conditionCode = tokens[14];

        if (exclusionPeriods(bidPrice, askPrice, tradeVolume, conditionCode)) {
            continue;
        }

        string stockId = tokens[0];
        double tradePrice = stod(tokens[3]);
        int updateType = stoi(tokens[8]);
        int time = stoi(tokens[11]); // Timestamp

        stockData[stockId].emplace_back(bidPrice, askPrice, tradePrice, tradeVolume, time, conditionCode, updateType);
    }
    infile.close();

    // Open output CSV file
    ofstream outfile("output2.csv");

    // Write header to output file
    outfile << "Stock ID,Mean Time Between Trades,Median Time Between Trades,Mean Time Between Tick Changes,Median Time Between Tick Changes,"
               "Longest Time Between Trades,Longest Time Between Tick Changes,Mean Bid Ask Spread,Median Bid Ask Spread,"
               "Price Round Number Effect,Volume Round Number Effect,Mean Time Between Type Changes,Median between Type Changes\n";

    // Process each stock's data for analysis
    for (const auto &entry : stockData) {
        const string &stockId = entry.first;
        const vector<tuple<double, double, double, int, int, string, int>> &data = entry.second;

        vector<double> tradeTimes, tickTimes, bidAskSpreads, typeTimes, tradePrice;
        double zeroAsPriceLastDigit_Count = 0.0, nonzeroAsPriceLastDigit_Count = 0.0;
        double zeroAsVolumeLastDigit_Count = 0.0, nonzeroAsVolumeLastDigit_Count = 0.0;
        double timePassTickChange = 0.0;
        double timePassTypeChange = 0.0;

        // Process the data for the current stock
        for (size_t i = 1; i < data.size(); ++i) {
            // Extract current and previous data for comparison
            const auto &prev = data[i - 1];
            const auto &curr = data[i];

            // Time difference between trades
            double tradeTimeDiff = static_cast<double>(get<4>(curr) - get<4>(prev));
            tradeTimes.push_back(tradeTimeDiff);

            // Time difference between tick changes
            if (get<2>(curr) == get<2>(prev)) {
                timePassTickChange += tradeTimeDiff;
            } else {
                tickTimes.push_back(timePassTickChange);
                timePassTickChange = 0.0;
            }

            // Time difference between type changes
            if (get<6>(curr) == get<6>(prev)) {
                timePassTypeChange += tradeTimeDiff;
            } else {
                typeTimes.push_back(timePassTypeChange);
                timePassTypeChange = 0.0;
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

        // Calculate metrics
        double meanTradeTime = mean(tradeTimes);
        double medianTradeTime = median(tradeTimes);
        double meanTickTime = mean(tickTimes);
        double medianTickTime = median(tickTimes);
        double longestTradeTime = tradeTimes.empty() ? 0.0 : *max_element(tradeTimes.begin(), tradeTimes.end());
        double longestTickTime = tickTimes.empty() ? 0.0 : *max_element(tickTimes.begin(), tickTimes.end());
        double meanBidAskSpread = mean(bidAskSpreads);
        double medianBidAskSpread = median(bidAskSpreads);
        double meanTypeTime = mean(typeTimes);
        double medianTypeTime = median(typeTimes);

        // Round number effect (percentage of times the last digit is zero)
        double totalTradeCount = zeroAsPriceLastDigit_Count + nonzeroAsPriceLastDigit_Count;
        double priceRoundNumberEffect = totalTradeCount == 0 ? 0.0 : zeroAsPriceLastDigit_Count / totalTradeCount;

        double volumeRoundNumberEffect = totalTradeCount == 0 ? 0.0 : zeroAsVolumeLastDigit_Count / totalTradeCount;

        // Write to output file
        outfile << stockId << ","
                << meanTradeTime << "," << medianTradeTime << "," 
                << meanTickTime << "," << medianTickTime << ","
                << longestTradeTime << "," << longestTickTime << ","
                << meanBidAskSpread << "," << medianBidAskSpread << ","
                << priceRoundNumberEffect << "," << volumeRoundNumberEffect << ","
                << meanTypeTime << "," << medianTypeTime << "\n";
    }

    outfile.close();
    cout << "Processing complete. Output file closed.\n";
    return 0;
}
