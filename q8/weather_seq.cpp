#include <iostream>
#include <fstream>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <iomanip>
#include <limits>
#include <cstdint>

using namespace std;

struct Measurement {
    long long timestamp;
    int station_id;

    double temperature;
    double humidity;
    double pressure;
    double rainfall;
    double wind_speed;
};

struct StationStats {
    long long count = 0;

    double temperature_sum = 0.0;
    double rainfall_sum = 0.0;
};

struct ExtremeMeasurement {
    double temperature;
    int station_id;
    long long timestamp;
};

int main(int argc, char* argv[]) {

    if (argc != 2) {
        cerr << "Usage: " << argv[0] << " <input_file>\n";
        return 1;
    }

    string input_file = argv[1];

    ifstream fin(input_file);

    if (!fin) {
        cerr << "Error: could not open input file\n";
        return 1;
    }

    // ------------------------------------------------------------
    // Read N K S
    // ------------------------------------------------------------

    long long N;
    int K;
    int S;

    fin >> N >> K >> S;

    if (!fin || N <= 0 || K <= 0 || S <= 0 || K > S) {
        cerr << "Error: invalid input header\n";
        return 1;
    }

    // ------------------------------------------------------------
    // Global statistics
    // ------------------------------------------------------------

    long long total_measurements = 0;

    double temperature_sum = 0.0;
    double humidity_sum = 0.0;
    double pressure_sum = 0.0;

    double rainfall_total = 0.0;
    double wind_speed_sum = 0.0;

    double min_temperature = numeric_limits<double>::infinity();
    double max_temperature = -numeric_limits<double>::infinity();

    double min_humidity = numeric_limits<double>::infinity();
    double max_humidity = -numeric_limits<double>::infinity();

    double min_pressure = numeric_limits<double>::infinity();
    double max_pressure = -numeric_limits<double>::infinity();

    double max_rainfall = -numeric_limits<double>::infinity();
    double max_wind_speed = -numeric_limits<double>::infinity();

    long long extreme_temperature_events = 0;

    // ------------------------------------------------------------
    // Station statistics
    //
    // Since station_id is in [0, S-1], a vector is more efficient
    // than an unordered_map.
    // ------------------------------------------------------------

    vector<StationStats> stations(S);

    // ------------------------------------------------------------
    // 60-second interval statistics
    //
    // interval_id = timestamp / 60
    // ------------------------------------------------------------

    unordered_map<long long, long long> interval_count;

    // ------------------------------------------------------------
    // Hottest / coldest measurement
    //
    // Hottest:
    //   larger temperature wins
    //   if tied -> smaller timestamp
    //   if still tied -> smaller station ID
    //
    // Coldest:
    //   smaller temperature wins
    //   if tied -> smaller timestamp
    //   if still tied -> smaller station ID
    // ------------------------------------------------------------

    ExtremeMeasurement hottest;
    ExtremeMeasurement coldest;

    hottest.temperature = -numeric_limits<double>::infinity();
    hottest.station_id = numeric_limits<int>::max();
    hottest.timestamp = numeric_limits<long long>::max();

    coldest.temperature = numeric_limits<double>::infinity();
    coldest.station_id = numeric_limits<int>::max();
    coldest.timestamp = numeric_limits<long long>::max();

    // ------------------------------------------------------------
    // Read and process measurements
    // ------------------------------------------------------------

    for (long long i = 0; i < N; i++) {

        Measurement m;

        fin >> m.timestamp
            >> m.station_id
            >> m.temperature
            >> m.humidity
            >> m.pressure
            >> m.rainfall
            >> m.wind_speed;

        if (!fin) {
            cerr << "Error: invalid measurement at record " << i << "\n";
            return 1;
        }

        // --------------------------------------------------------
        // Validate station ID
        // station_id must be in [0, S-1]
        // --------------------------------------------------------

        if (m.station_id < 0 || m.station_id >= S) {
            cerr << "Error: station_id " << m.station_id
                << " out of range [0, " << S - 1 << "]\n";
            return 1;
        }

        // --------------------------------------------------------
        // Basic global statistics
        // --------------------------------------------------------

        total_measurements++;

        temperature_sum += m.temperature;
        humidity_sum += m.humidity;
        pressure_sum += m.pressure;

        rainfall_total += m.rainfall;
        wind_speed_sum += m.wind_speed;

        min_temperature = min(min_temperature, m.temperature);
        max_temperature = max(max_temperature, m.temperature);

        min_humidity = min(min_humidity, m.humidity);
        max_humidity = max(max_humidity, m.humidity);

        min_pressure = min(min_pressure, m.pressure);
        max_pressure = max(max_pressure, m.pressure);

        max_rainfall = max(max_rainfall, m.rainfall);
        max_wind_speed = max(max_wind_speed, m.wind_speed);

        // --------------------------------------------------------
        // Extreme temperature events
        //
        // temperature >= 40 OR temperature <= 0
        // --------------------------------------------------------

        if (m.temperature >= 40.0 || m.temperature <= 0.0) {
            extreme_temperature_events++;
        }

        // --------------------------------------------------------
        // Station statistics
        // --------------------------------------------------------

        stations[m.station_id].count++;
        stations[m.station_id].temperature_sum += m.temperature;
        stations[m.station_id].rainfall_sum += m.rainfall;

        // --------------------------------------------------------
        // Busiest 60-second interval
        // --------------------------------------------------------

        long long interval_id = m.timestamp / 60;
        interval_count[interval_id]++;

        // --------------------------------------------------------
        // Hottest measurement
        // --------------------------------------------------------

        bool hotter = false;

        if (m.temperature > hottest.temperature) {
            hotter = true;
        }
        else if (m.temperature == hottest.temperature) {

            if (m.timestamp < hottest.timestamp) {
                hotter = true;
            }
            else if (m.timestamp == hottest.timestamp &&
                     m.station_id < hottest.station_id) {
                hotter = true;
            }
        }

        if (hotter) {
            hottest.temperature = m.temperature;
            hottest.station_id = m.station_id;
            hottest.timestamp = m.timestamp;
        }

        // --------------------------------------------------------
        // Coldest measurement
        // --------------------------------------------------------

        bool colder = false;

        if (m.temperature < coldest.temperature) {
            colder = true;
        }
        else if (m.temperature == coldest.temperature) {

            if (m.timestamp < coldest.timestamp) {
                colder = true;
            }
            else if (m.timestamp == coldest.timestamp &&
                     m.station_id < coldest.station_id) {
                colder = true;
            }
        }

        if (colder) {
            coldest.temperature = m.temperature;
            coldest.station_id = m.station_id;
            coldest.timestamp = m.timestamp;
        }
    }

    // ------------------------------------------------------------
    // Check for extra input after the expected N measurements
    // ------------------------------------------------------------

    string extra;

    if (fin >> extra) {
        cerr << "Error: extra data found after "
            << N << " measurements\n";
        return 1;
    }

    fin.close();

    // ------------------------------------------------------------
    // Calculate averages
    // ------------------------------------------------------------

    double average_temperature =
        temperature_sum / total_measurements;

    double average_humidity =
        humidity_sum / total_measurements;

    double average_pressure =
        pressure_sum / total_measurements;

    double average_wind_speed =
        wind_speed_sum / total_measurements;

    // ------------------------------------------------------------
    // Find busiest interval
    //
    // Maximum count wins.
    // If tied, smaller interval ID wins.
    // ------------------------------------------------------------

    long long busiest_interval_id = numeric_limits<long long>::max();
    long long busiest_interval_count = -1;

    for (const auto& entry : interval_count) {

        long long interval_id = entry.first;
        long long count = entry.second;

        if (count > busiest_interval_count) {
            busiest_interval_count = count;
            busiest_interval_id = interval_id;
        }
        else if (count == busiest_interval_count &&
                 interval_id < busiest_interval_id) {
            busiest_interval_id = interval_id;
        }
    }

    // ------------------------------------------------------------
    // Build Top-K station list
    // ------------------------------------------------------------

    vector<int> station_ids(S);

    for (int i = 0; i < S; i++) {
        station_ids[i] = i;
    }

    sort(
        station_ids.begin(),
        station_ids.end(),
        [&](int a, int b) {

            // Decreasing measurement count
            if (stations[a].count != stations[b].count) {
                return stations[a].count > stations[b].count;
            }

            // Increasing station ID
            return a < b;
        }
    );

    // ------------------------------------------------------------
    // Output
    // ------------------------------------------------------------

    cout << fixed << setprecision(6);

    cout << "TOTAL_MEASUREMENTS "
         << total_measurements << "\n";

    cout << "AVERAGE_TEMPERATURE "
         << average_temperature << "\n";

    cout << "MIN_TEMPERATURE "
         << min_temperature << "\n";

    cout << "MAX_TEMPERATURE "
         << max_temperature << "\n";

    cout << "AVERAGE_HUMIDITY "
         << average_humidity << "\n";

    cout << "MIN_HUMIDITY "
         << min_humidity << "\n";

    cout << "MAX_HUMIDITY "
         << max_humidity << "\n";

    cout << "AVERAGE_PRESSURE "
         << average_pressure << "\n";

    cout << "MIN_PRESSURE "
         << min_pressure << "\n";

    cout << "MAX_PRESSURE "
         << max_pressure << "\n";

    cout << "TOTAL_RAINFALL "
         << rainfall_total << "\n";

    cout << "MAX_RAINFALL "
         << max_rainfall << "\n";

    cout << "AVERAGE_WIND_SPEED "
         << average_wind_speed << "\n";

    cout << "MAX_WIND_SPEED "
         << max_wind_speed << "\n";

    cout << "EXTREME_TEMPERATURE_EVENTS "
         << extreme_temperature_events << "\n";

    cout << "HOTTEST_MEASUREMENT "
         << hottest.temperature << " "
         << hottest.station_id << " "
         << hottest.timestamp << "\n";

    cout << "COLDEST_MEASUREMENT "
         << coldest.temperature << " "
         << coldest.station_id << " "
         << coldest.timestamp << "\n";

    cout << "BUSIEST_INTERVAL "
         << busiest_interval_id << " "
         << busiest_interval_count << "\n";

    cout << "TOP_STATIONS\n";

    int top_k = min(K, S);

    for (int i = 0; i < top_k; i++) {

        int station_id = station_ids[i];

        double average_station_temperature =
            stations[station_id].temperature_sum /
            stations[station_id].count;

        cout << station_id << " "
             << stations[station_id].count << " "
             << average_station_temperature << " "
             << stations[station_id].rainfall_sum << "\n";
    }

    return 0;
}