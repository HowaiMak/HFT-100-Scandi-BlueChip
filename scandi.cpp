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
    sort(values.begin(), values.end());
    if (values.size() % 2 == 0) {
        return (values[values.size() / 2 - 1] + values[values.size() / 2]) / 2.0;
    }
    return values[values.size() / 2];
}

// Function to compute the bid-ask spread
inline double computeBidAskSpread(double askPrice, double bidPrice) {
    return askPrice - bidPrice; 
}

// Function to safely convert string to double with default value
double safe_stod(const string& str, double default_value = 0.0) {
    if (str.empty()) return default_value;
    try {
        return stod(str);
    } catch (const exception& e) {
        return default_value;
    }
}

// Function to safely convert string to int with default value
int safe_stoi(const string& str, int default_value = 0) {
    if (str.empty()) return default_value;
    try {
        return stoi(str);
    } catch (const exception& e) {
        return default_value;
    }
}

// Function to check if the condition indicates an auction period
bool exclusionPeriods(const double &bidPrice, const double &askPrice, const int &tradeVolume, const string &conditionCode) {
    return (tradeVolume == 0 || (bidPrice > askPrice && conditionCode != "XT" && conditionCode != ""));
}

int main() {
    // Open input CSV file containing tick data
    ifstream infile("scandi/scandi.csv");
    if (!infile.is_open()) {
        cerr << "Error: Could not open input file!" << endl;
        return 1;
    }
    cout << "Reading data file.\n";

    // Map to store stock data (keyed by stock ID)
    map<string, vector<tuple<double, double, double, int, int, string, int>>> stockData;
    string line;

    // Read and process CSV data line by line
    while (getline(infile, line)) {
        auto tokens = split(line, ','); // Using comma delimiter

        if (tokens.size() < 16) { // Need at least 16 columns for condition codes
            // Pad with empty strings if we don't have enough columns
            while (tokens.size() < 16) {
                tokens.push_back("");
            }
        }

        try {
            // CORRECTED INDICES based on your data description
            string stockId = tokens[0];               // Column 1: Bloomberg Code/Stock identifier
            double bidPrice = safe_stod(tokens[2]);   // Column 3: Bid Price
            double askPrice = safe_stod(tokens[3]);   // Column 4: Ask Price
            double tradePrice = safe_stod(tokens[4]); // Column 5: Trade Price
            int tradeVolume = safe_stoi(tokens[7]);   // Column 8: Trade Volume
            int updateType = safe_stoi(tokens[8]);    // Column 9: Update type
            int time = safe_stoi(tokens[11]);         // Column 12: Time in seconds past midnight
            string conditionCode = tokens[14];        // Column 15: Condition codes

            if (exclusionPeriods(bidPrice, askPrice, tradeVolume, conditionCode)) {
                continue;
            }

            stockData[stockId].emplace_back(bidPrice, askPrice, tradePrice, tradeVolume, time, conditionCode, updateType);
        } catch (const exception& e) {
            cerr << "Error parsing line: " << line << " - " << e.what() << endl;
            continue;
        }
    }
    infile.close();

    // Open output CSV file
    cout << "Processing Output file.\n";
    ofstream outfile("output.csv");
    if (!outfile.is_open()) {
        cerr << "Error: Could not create output file!" << endl;
        return 1;
    }

    // Write header to output file
    outfile << "Stock ID,Mean Time Between Trades,Median Time Between Trades,Mean Time Between Tick Changes,Median Time Between Tick Changes,"
               "Longest Time Between Trades,Longest Time Between Tick Changes,Mean Bid Ask Spread,Median Bid Ask Spread,"
               "Price Round Number Effect,Volume Round Number Effect,Mean Time Between Type Changes,Median between Type Changes\n";

    // Process each stock's data for analysis
    for (const auto &entry : stockData) {
        const string &stockId = entry.first;
        const vector<tuple<double, double, double, int, int, string, int>> &data = entry.second;

        if (data.empty()) {
            continue;
        }

        vector<double> tradeTimes, tickTimes, bidAskSpreads, typeTimes;
        double zeroAsPriceLastDigit_Count = 0.0, nonzeroAsPriceLastDigit_Count = 0.0;
        double zeroAsVolumeLastDigit_Count = 0.0, nonzeroAsVolumeLastDigit_Count = 0.0;

        // For time between changes calculations
        int lastTradeTime = get<4>(data[0]);
        int lastTickChangeTime = get<4>(data[0]);
        int lastTypeChangeTime = get<4>(data[0]);

        // Process the data for the current stock
        for (size_t i = 1; i < data.size(); ++i) {
            const auto &prev = data[i - 1];
            const auto &curr = data[i];

            int currentTime = get<4>(curr);

            // Calculate bid-ask spread for current quote
            bidAskSpreads.push_back(computeBidAskSpread(get<1>(curr), get<0>(curr)));

            // Check if this is a trade (updateType = 1)
            int currTradeVolume = get<3>(curr);
            int currUpdateType = get<6>(curr);
            
            if (currUpdateType == 1) { // Only process actual trades
                double tradeTimeDiff = static_cast<double>(currentTime - lastTradeTime);
                
                // Calculate time between trades
                tradeTimes.push_back(tradeTimeDiff);
                lastTradeTime = currentTime;

                // Volume round number effect (only for actual trades)
                if (currTradeVolume % 10 == 0) {
                    zeroAsVolumeLastDigit_Count++;
                } else {
                    nonzeroAsVolumeLastDigit_Count++;
                }

                // Price round number effect (only for actual trades)
                double currTradePrice = get<2>(curr);
                if (static_cast<int>(currTradePrice) % 10 == 0) {
                    zeroAsPriceLastDigit_Count++;
                } else {
                    nonzeroAsPriceLastDigit_Count++;
                }
            }

            // Time between tick changes (price changes)
            if (get<2>(curr) != get<2>(prev)) {
                double timeSinceLastTickChange = static_cast<double>(currentTime - lastTickChangeTime);
                if (timeSinceLastTickChange > 0) { // Avoid zero intervals
                    tickTimes.push_back(timeSinceLastTickChange);
                }
                lastTickChangeTime = currentTime;
            }

            // Time between type changes (updateType changes)
            if (get<6>(curr) != get<6>(prev)) {
                double timeSinceLastTypeChange = static_cast<double>(currentTime - lastTypeChangeTime);
                if (timeSinceLastTypeChange > 0) { // Avoid zero intervals
                    typeTimes.push_back(timeSinceLastTypeChange);
                }
                lastTypeChangeTime = currentTime;
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

        // Round number effects
        double totalTradeCount = zeroAsPriceLastDigit_Count + nonzeroAsPriceLastDigit_Count;
        double priceRoundNumberEffect = totalTradeCount == 0 ? 0.0 : zeroAsPriceLastDigit_Count / totalTradeCount;

        double totalVolumeTradeCount = zeroAsVolumeLastDigit_Count + nonzeroAsVolumeLastDigit_Count;
        double volumeRoundNumberEffect = totalVolumeTradeCount == 0 ? 0.0 : zeroAsVolumeLastDigit_Count / totalVolumeTradeCount;

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
    cout << "Processing complete. Output file 'output.csv' created.\n";
    return 0;
}