#include <mpi.h>

#include <iostream>
#include <fstream>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <iomanip>
#include <limits>
#include <string>
#include <cstdlib>
#include <cstddef>

using namespace std;


// ============================================================
// Data structures
// ============================================================

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


// ============================================================
// Helper functions
// ============================================================

// ------------------------------------------------------------
// Hottest comparison
//
// Larger temperature wins.
// If tied -> smaller timestamp.
// If still tied -> smaller station ID.
// ------------------------------------------------------------

bool isHotter(const ExtremeMeasurement& a,
              const ExtremeMeasurement& b) {

    if (a.temperature != b.temperature) {
        return a.temperature > b.temperature;
    }

    if (a.timestamp != b.timestamp) {
        return a.timestamp < b.timestamp;
    }

    return a.station_id < b.station_id;
}


// ------------------------------------------------------------
// Coldest comparison
//
// Smaller temperature wins.
// If tied -> smaller timestamp.
// If still tied -> smaller station ID.
// ------------------------------------------------------------

bool isColder(const ExtremeMeasurement& a,
              const ExtremeMeasurement& b) {

    if (a.temperature != b.temperature) {
        return a.temperature < b.temperature;
    }

    if (a.timestamp != b.timestamp) {
        return a.timestamp < b.timestamp;
    }

    return a.station_id < b.station_id;
}


// ============================================================
// Main
// ============================================================

int main(int argc, char* argv[]) {

    MPI_Init(&argc, &argv);

    int rank;
    int world_size;

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);


    // ========================================================
    // Command-line validation
    // ========================================================

    if (argc != 3) {

        if (rank == 0) {
            cerr << "Usage: " << argv[0]
                 << " <input_file> <timing_file>\n";
        }

        MPI_Finalize();
        return 1;
    }


    string input_file = argv[1];
    string timing_file = argv[2];


    // ========================================================
    // Variables that must be known by every process
    // ========================================================

    long long N = 0;
    int K = 0;
    int S = 0;

    vector<Measurement> all_measurements;

    int input_valid = 1;


    // ========================================================
    // Rank 0 reads and validates the input
    // ========================================================

    if (rank == 0) {

        ifstream fin(input_file);

        if (!fin) {

            cerr << "Error: could not open input file\n";
            input_valid = 0;
        }


        if (input_valid) {

            // ------------------------------------------------
            // Read N K S
            // ------------------------------------------------

            fin >> N >> K >> S;

            if (!fin || N <= 0 || K <= 0 ||
                S <= 0 || K > S) {

                cerr << "Error: invalid input header\n";
                input_valid = 0;
            }
        }


        // ----------------------------------------------------
        // Read all measurements
        // ----------------------------------------------------

        if (input_valid) {

            all_measurements.resize(
                static_cast<size_t>(N)
            );


            for (long long i = 0; i < N; i++) {

                Measurement& m = all_measurements[i];

                fin >> m.timestamp
                    >> m.station_id
                    >> m.temperature
                    >> m.humidity
                    >> m.pressure
                    >> m.rainfall
                    >> m.wind_speed;


                if (!fin) {

                    cerr << "Error: invalid measurement at "
                         << "record " << i << "\n";

                    input_valid = 0;
                    break;
                }


                // --------------------------------------------
                // Validate station ID
                // --------------------------------------------

                if (m.station_id < 0 ||
                    m.station_id >= S) {

                    cerr << "Error: station_id "
                         << m.station_id
                         << " out of range [0, "
                         << S - 1 << "]\n";

                    input_valid = 0;
                    break;
                }
            }
        }


        // ----------------------------------------------------
        // Check for extra input
        // ----------------------------------------------------

        if (input_valid) {

            string extra;

            if (fin >> extra) {

                cerr << "Error: extra data found after "
                     << N << " measurements\n";

                input_valid = 0;
            }
        }


        fin.close();
    }


    // ========================================================
    // Tell every rank whether input was valid
    // ========================================================

    MPI_Bcast(
        &input_valid,
        1,
        MPI_INT,
        0,
        MPI_COMM_WORLD
    );


    if (!input_valid) {

        MPI_Finalize();
        return 1;
    }


    // ========================================================
    // Broadcast N, K and S
    // ========================================================

    MPI_Bcast(
        &N,
        1,
        MPI_LONG_LONG,
        0,
        MPI_COMM_WORLD
    );

    MPI_Bcast(
        &K,
        1,
        MPI_INT,
        0,
        MPI_COMM_WORLD
    );

    MPI_Bcast(
        &S,
        1,
        MPI_INT,
        0,
        MPI_COMM_WORLD
    );


    // ========================================================
    // Build MPI datatype for Measurement
    // ========================================================

    MPI_Datatype MPI_MEASUREMENT;

    int block_lengths[7] = {
        1, 1, 1, 1, 1, 1, 1
    };

    MPI_Aint offsets[7];

    offsets[0] = offsetof(Measurement, timestamp);
    offsets[1] = offsetof(Measurement, station_id);
    offsets[2] = offsetof(Measurement, temperature);
    offsets[3] = offsetof(Measurement, humidity);
    offsets[4] = offsetof(Measurement, pressure);
    offsets[5] = offsetof(Measurement, rainfall);
    offsets[6] = offsetof(Measurement, wind_speed);


    MPI_Datatype types[7] = {
        MPI_LONG_LONG,
        MPI_INT,
        MPI_DOUBLE,
        MPI_DOUBLE,
        MPI_DOUBLE,
        MPI_DOUBLE,
        MPI_DOUBLE
    };


    MPI_Type_create_struct(
        7,
        block_lengths,
        offsets,
        types,
        &MPI_MEASUREMENT
    );

    MPI_Type_commit(&MPI_MEASUREMENT);


    // ========================================================
    // Divide N measurements among processes
    // ========================================================

    vector<int> send_counts(world_size);
    vector<int> displacements(world_size);

    int base_count =
        static_cast<int>(N / world_size);

    int remainder =
        static_cast<int>(N % world_size);


    for (int i = 0; i < world_size; i++) {

        send_counts[i] =
            base_count + (i < remainder ? 1 : 0);

        if (i == 0) {

            displacements[i] = 0;
        }
        else {

            displacements[i] =
                displacements[i - 1] +
                send_counts[i - 1];
        }
    }


    int local_count = send_counts[rank];


    vector<Measurement> local_measurements(local_count);


    // ========================================================
    // Total timing starts here
    //
    // Input file reading by Rank 0 is intentionally excluded.
    // ========================================================

    MPI_Barrier(MPI_COMM_WORLD);

    double total_start = MPI_Wtime();


    // ========================================================
    // Data distribution timing
    // ========================================================

    double distribution_start = MPI_Wtime();


    MPI_Scatterv(
        all_measurements.data(),
        send_counts.data(),
        displacements.data(),
        MPI_MEASUREMENT,

        local_measurements.data(),
        local_count,
        MPI_MEASUREMENT,

        0,
        MPI_COMM_WORLD
    );


    double distribution_end = MPI_Wtime();

    double local_distribution_time =
        distribution_end - distribution_start;


    double distribution_time = 0.0;


    MPI_Reduce(
        &local_distribution_time,
        &distribution_time,
        1,
        MPI_DOUBLE,
        MPI_MAX,
        0,
        MPI_COMM_WORLD
    );


    // Rank 0 no longer needs the complete input.
    if (rank == 0) {

        all_measurements.clear();
        all_measurements.shrink_to_fit();
    }


    // ========================================================
    // Local statistics
    // ========================================================

    long long local_total_measurements = 0;

    double local_temperature_sum = 0.0;
    double local_humidity_sum = 0.0;
    double local_pressure_sum = 0.0;

    double local_rainfall_total = 0.0;
    double local_wind_speed_sum = 0.0;


    double local_min_temperature =
        numeric_limits<double>::infinity();

    double local_max_temperature =
        -numeric_limits<double>::infinity();


    double local_min_humidity =
        numeric_limits<double>::infinity();

    double local_max_humidity =
        -numeric_limits<double>::infinity();


    double local_min_pressure =
        numeric_limits<double>::infinity();

    double local_max_pressure =
        -numeric_limits<double>::infinity();


    double local_max_rainfall =
        -numeric_limits<double>::infinity();

    double local_max_wind_speed =
        -numeric_limits<double>::infinity();


    long long local_extreme_temperature_events = 0;


    // ========================================================
    // Local station statistics
    // ========================================================

    vector<StationStats> local_stations(S);


    // ========================================================
    // Local interval statistics
    // ========================================================

    unordered_map<long long, long long> local_interval_count;


    // ========================================================
    // Local hottest / coldest
    // ========================================================

    ExtremeMeasurement local_hottest;

    local_hottest.temperature =
        -numeric_limits<double>::infinity();

    local_hottest.station_id =
        numeric_limits<int>::max();

    local_hottest.timestamp =
        numeric_limits<long long>::max();


    ExtremeMeasurement local_coldest;

    local_coldest.temperature =
        numeric_limits<double>::infinity();

    local_coldest.station_id =
        numeric_limits<int>::max();

    local_coldest.timestamp =
        numeric_limits<long long>::max();


    // ========================================================
    // Local computation timing
    // ========================================================

    MPI_Barrier(MPI_COMM_WORLD);

    double computation_start = MPI_Wtime();


    // ========================================================
    // Process local measurements
    // ========================================================

    for (const Measurement& m : local_measurements) {

        local_total_measurements++;


        // ----------------------------------------------------
        // Global statistics
        // ----------------------------------------------------

        local_temperature_sum += m.temperature;
        local_humidity_sum += m.humidity;
        local_pressure_sum += m.pressure;

        local_rainfall_total += m.rainfall;
        local_wind_speed_sum += m.wind_speed;


        local_min_temperature =
            min(local_min_temperature, m.temperature);

        local_max_temperature =
            max(local_max_temperature, m.temperature);


        local_min_humidity =
            min(local_min_humidity, m.humidity);

        local_max_humidity =
            max(local_max_humidity, m.humidity);


        local_min_pressure =
            min(local_min_pressure, m.pressure);

        local_max_pressure =
            max(local_max_pressure, m.pressure);


        local_max_rainfall =
            max(local_max_rainfall, m.rainfall);

        local_max_wind_speed =
            max(local_max_wind_speed, m.wind_speed);


        // ----------------------------------------------------
        // Extreme temperature events
        // ----------------------------------------------------

        if (m.temperature >= 40.0 ||
            m.temperature <= 0.0) {

            local_extreme_temperature_events++;
        }


        // ----------------------------------------------------
        // Station statistics
        // ----------------------------------------------------

        local_stations[m.station_id].count++;

        local_stations[m.station_id]
            .temperature_sum += m.temperature;

        local_stations[m.station_id]
            .rainfall_sum += m.rainfall;


        // ----------------------------------------------------
        // 60-second interval
        // ----------------------------------------------------

        long long interval_id =
            m.timestamp / 60;

        local_interval_count[interval_id]++;


        // ----------------------------------------------------
        // Hottest measurement
        // ----------------------------------------------------

        ExtremeMeasurement current;

        current.temperature = m.temperature;
        current.station_id = m.station_id;
        current.timestamp = m.timestamp;


        if (isHotter(current, local_hottest)) {

            local_hottest = current;
        }


        // ----------------------------------------------------
        // Coldest measurement
        // ----------------------------------------------------

        if (isColder(current, local_coldest)) {

            local_coldest = current;
        }
    }


    double computation_end = MPI_Wtime();

    double local_computation_time =
        computation_end - computation_start;


    double computation_time = 0.0;


    MPI_Reduce(
        &local_computation_time,
        &computation_time,
        1,
        MPI_DOUBLE,
        MPI_MAX,
        0,
        MPI_COMM_WORLD
    );


    // ========================================================
    // Global aggregation timing
    // ========================================================

    MPI_Barrier(MPI_COMM_WORLD);

    double aggregation_start = MPI_Wtime();


    // ========================================================
    // Reduce global scalar statistics
    // ========================================================

    long long total_measurements = 0;

    double temperature_sum = 0.0;
    double humidity_sum = 0.0;
    double pressure_sum = 0.0;

    double rainfall_total = 0.0;
    double wind_speed_sum = 0.0;


    double min_temperature;
    double max_temperature;

    double min_humidity;
    double max_humidity;

    double min_pressure;
    double max_pressure;

    double max_rainfall;
    double max_wind_speed;


    long long extreme_temperature_events = 0;


    MPI_Reduce(
        &local_total_measurements,
        &total_measurements,
        1,
        MPI_LONG_LONG,
        MPI_SUM,
        0,
        MPI_COMM_WORLD
    );


    MPI_Reduce(
        &local_temperature_sum,
        &temperature_sum,
        1,
        MPI_DOUBLE,
        MPI_SUM,
        0,
        MPI_COMM_WORLD
    );


    MPI_Reduce(
        &local_humidity_sum,
        &humidity_sum,
        1,
        MPI_DOUBLE,
        MPI_SUM,
        0,
        MPI_COMM_WORLD
    );


    MPI_Reduce(
        &local_pressure_sum,
        &pressure_sum,
        1,
        MPI_DOUBLE,
        MPI_SUM,
        0,
        MPI_COMM_WORLD
    );


    MPI_Reduce(
        &local_rainfall_total,
        &rainfall_total,
        1,
        MPI_DOUBLE,
        MPI_SUM,
        0,
        MPI_COMM_WORLD
    );


    MPI_Reduce(
        &local_wind_speed_sum,
        &wind_speed_sum,
        1,
        MPI_DOUBLE,
        MPI_SUM,
        0,
        MPI_COMM_WORLD
    );


    MPI_Reduce(
        &local_min_temperature,
        &min_temperature,
        1,
        MPI_DOUBLE,
        MPI_MIN,
        0,
        MPI_COMM_WORLD
    );


    MPI_Reduce(
        &local_max_temperature,
        &max_temperature,
        1,
        MPI_DOUBLE,
        MPI_MAX,
        0,
        MPI_COMM_WORLD
    );


    MPI_Reduce(
        &local_min_humidity,
        &min_humidity,
        1,
        MPI_DOUBLE,
        MPI_MIN,
        0,
        MPI_COMM_WORLD
    );


    MPI_Reduce(
        &local_max_humidity,
        &max_humidity,
        1,
        MPI_DOUBLE,
        MPI_MAX,
        0,
        MPI_COMM_WORLD
    );


    MPI_Reduce(
        &local_min_pressure,
        &min_pressure,
        1,
        MPI_DOUBLE,
        MPI_MIN,
        0,
        MPI_COMM_WORLD
    );


    MPI_Reduce(
        &local_max_pressure,
        &max_pressure,
        1,
        MPI_DOUBLE,
        MPI_MAX,
        0,
        MPI_COMM_WORLD
    );


    MPI_Reduce(
        &local_max_rainfall,
        &max_rainfall,
        1,
        MPI_DOUBLE,
        MPI_MAX,
        0,
        MPI_COMM_WORLD
    );


    MPI_Reduce(
        &local_max_wind_speed,
        &max_wind_speed,
        1,
        MPI_DOUBLE,
        MPI_MAX,
        0,
        MPI_COMM_WORLD
    );


    MPI_Reduce(
        &local_extreme_temperature_events,
        &extreme_temperature_events,
        1,
        MPI_LONG_LONG,
        MPI_SUM,
        0,
        MPI_COMM_WORLD
    );


    // ========================================================
    // Reduce station statistics
    // ========================================================

    vector<long long> local_station_counts(S);
    vector<double> local_station_temperature_sums(S);
    vector<double> local_station_rainfall_sums(S);


    for (int i = 0; i < S; i++) {

        local_station_counts[i] =
            local_stations[i].count;

        local_station_temperature_sums[i] =
            local_stations[i].temperature_sum;

        local_station_rainfall_sums[i] =
            local_stations[i].rainfall_sum;
    }


    vector<long long> station_counts(S);
    vector<double> station_temperature_sums(S);
    vector<double> station_rainfall_sums(S);


    MPI_Reduce(
        local_station_counts.data(),
        station_counts.data(),
        S,
        MPI_LONG_LONG,
        MPI_SUM,
        0,
        MPI_COMM_WORLD
    );


    MPI_Reduce(
        local_station_temperature_sums.data(),
        station_temperature_sums.data(),
        S,
        MPI_DOUBLE,
        MPI_SUM,
        0,
        MPI_COMM_WORLD
    );


    MPI_Reduce(
        local_station_rainfall_sums.data(),
        station_rainfall_sums.data(),
        S,
        MPI_DOUBLE,
        MPI_SUM,
        0,
        MPI_COMM_WORLD
    );


    // ========================================================
    // Gather hottest / coldest candidates
    // ========================================================

    ExtremeMeasurement hottest;
    ExtremeMeasurement coldest;


    vector<ExtremeMeasurement> all_hottest;
    vector<ExtremeMeasurement> all_coldest;


    if (rank == 0) {

        all_hottest.resize(world_size);
        all_coldest.resize(world_size);
    }


    MPI_Datatype MPI_EXTREME;

    int extreme_block_lengths[3] = {
        1, 1, 1
    };

    MPI_Aint extreme_offsets[3];

    extreme_offsets[0] =
        offsetof(ExtremeMeasurement, temperature);

    extreme_offsets[1] =
        offsetof(ExtremeMeasurement, station_id);

    extreme_offsets[2] =
        offsetof(ExtremeMeasurement, timestamp);


    MPI_Datatype extreme_types[3] = {
        MPI_DOUBLE,
        MPI_INT,
        MPI_LONG_LONG
    };


    MPI_Type_create_struct(
        3,
        extreme_block_lengths,
        extreme_offsets,
        extreme_types,
        &MPI_EXTREME
    );

    MPI_Type_commit(&MPI_EXTREME);


    MPI_Gather(
        &local_hottest,
        1,
        MPI_EXTREME,

        all_hottest.data(),
        1,
        MPI_EXTREME,

        0,
        MPI_COMM_WORLD
    );


    MPI_Gather(
        &local_coldest,
        1,
        MPI_EXTREME,

        all_coldest.data(),
        1,
        MPI_EXTREME,

        0,
        MPI_COMM_WORLD
    );


    // ========================================================
    // Gather local interval maps
    // ========================================================

    vector<long long> local_interval_ids;
    vector<long long> local_interval_counts;


    for (const auto& entry : local_interval_count) {

        local_interval_ids.push_back(entry.first);
        local_interval_counts.push_back(entry.second);
    }


    int local_interval_size =
        static_cast<int>(local_interval_ids.size());


    vector<int> interval_sizes;

    if (rank == 0) {
        interval_sizes.resize(world_size);
    }


    MPI_Gather(
        &local_interval_size,
        1,
        MPI_INT,

        interval_sizes.data(),
        1,
        MPI_INT,

        0,
        MPI_COMM_WORLD
    );


    vector<int> interval_displacements;

    vector<long long> all_interval_ids;
    vector<long long> all_interval_counts;


    if (rank == 0) {

        interval_displacements.resize(world_size);

        int total_intervals = 0;


        for (int i = 0; i < world_size; i++) {

            interval_displacements[i] =
                total_intervals;

            total_intervals += interval_sizes[i];
        }


        all_interval_ids.resize(total_intervals);
        all_interval_counts.resize(total_intervals);
    }


    MPI_Gatherv(
        local_interval_ids.data(),
        local_interval_size,
        MPI_LONG_LONG,

        all_interval_ids.data(),
        interval_sizes.data(),
        interval_displacements.data(),
        MPI_LONG_LONG,

        0,
        MPI_COMM_WORLD
    );


    MPI_Gatherv(
        local_interval_counts.data(),
        local_interval_size,
        MPI_LONG_LONG,

        all_interval_counts.data(),
        interval_sizes.data(),
        interval_displacements.data(),
        MPI_LONG_LONG,

        0,
        MPI_COMM_WORLD
    );


    // ========================================================
    // End aggregation timing
    // ========================================================

    double aggregation_end = MPI_Wtime();

    double local_aggregation_time =
        aggregation_end - aggregation_start;


    double aggregation_time = 0.0;


    MPI_Reduce(
        &local_aggregation_time,
        &aggregation_time,
        1,
        MPI_DOUBLE,
        MPI_MAX,
        0,
        MPI_COMM_WORLD
    );


    // ========================================================
    // Rank 0 final processing
    // ========================================================

    double final_processing_start = 0.0;
    double final_processing_end = 0.0;


    if (rank == 0) {

        final_processing_start = MPI_Wtime();


        // ----------------------------------------------------
        // Averages
        // ----------------------------------------------------

        double average_temperature =
            temperature_sum /
            total_measurements;

        double average_humidity =
            humidity_sum /
            total_measurements;

        double average_pressure =
            pressure_sum /
            total_measurements;

        double average_wind_speed =
            wind_speed_sum /
            total_measurements;


        // ----------------------------------------------------
        // Hottest measurement
        // ----------------------------------------------------

        hottest = all_hottest[0];

        for (int i = 1; i < world_size; i++) {

            if (isHotter(all_hottest[i], hottest)) {
                hottest = all_hottest[i];
            }
        }


        // ----------------------------------------------------
        // Coldest measurement
        // ----------------------------------------------------

        coldest = all_coldest[0];

        for (int i = 1; i < world_size; i++) {

            if (isColder(all_coldest[i], coldest)) {
                coldest = all_coldest[i];
            }
        }


        // ----------------------------------------------------
        // Merge interval counts
        // ----------------------------------------------------

        unordered_map<long long, long long> interval_count;


        for (size_t i = 0;
             i < all_interval_ids.size();
             i++) {

            interval_count[
                all_interval_ids[i]
            ] += all_interval_counts[i];
        }


        // ----------------------------------------------------
        // Find busiest interval
        // ----------------------------------------------------

        long long busiest_interval_id =
            numeric_limits<long long>::max();

        long long busiest_interval_count = -1;


        for (const auto& entry : interval_count) {

            long long interval_id = entry.first;
            long long count = entry.second;


            if (count > busiest_interval_count) {

                busiest_interval_count = count;
                busiest_interval_id = interval_id;
            }
            else if (
                count == busiest_interval_count &&
                interval_id < busiest_interval_id
            ) {

                busiest_interval_id = interval_id;
            }
        }


        // ----------------------------------------------------
        // Build station vector
        // ----------------------------------------------------

        vector<StationStats> stations(S);


        for (int i = 0; i < S; i++) {

            stations[i].count =
                station_counts[i];

            stations[i].temperature_sum =
                station_temperature_sums[i];

            stations[i].rainfall_sum =
                station_rainfall_sums[i];
        }


        // ----------------------------------------------------
        // Build Top-K station list
        // ----------------------------------------------------

        vector<int> station_ids(S);


        for (int i = 0; i < S; i++) {
            station_ids[i] = i;
        }


        sort(
            station_ids.begin(),
            station_ids.end(),

            [&](int a, int b) {

                if (stations[a].count !=
                    stations[b].count) {

                    return stations[a].count >
                           stations[b].count;
                }

                return a < b;
            }
        );


        // ----------------------------------------------------
        // Output
        // ----------------------------------------------------

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

            int station_id =
                station_ids[i];


            double average_station_temperature =
                stations[station_id]
                    .temperature_sum /
                stations[station_id].count;


            cout << station_id << " "
                 << stations[station_id].count << " "
                 << average_station_temperature << " "
                 << stations[station_id].rainfall_sum
                 << "\n";
        }


        final_processing_end = MPI_Wtime();
    }


    // ========================================================
    // Final processing timing
    // ========================================================

    double local_final_processing_time = 0.0;


    if (rank == 0) {

        local_final_processing_time =
            final_processing_end -
            final_processing_start;
    }


    double final_processing_time = 0.0;


    MPI_Reduce(
        &local_final_processing_time,
        &final_processing_time,
        1,
        MPI_DOUBLE,
        MPI_MAX,
        0,
        MPI_COMM_WORLD
    );


    // ========================================================
    // Total timing
    // ========================================================

    MPI_Barrier(MPI_COMM_WORLD);

    double total_end = MPI_Wtime();

    double local_total_time =
        total_end - total_start;


    double total_time = 0.0;


    MPI_Reduce(
        &local_total_time,
        &total_time,
        1,
        MPI_DOUBLE,
        MPI_MAX,
        0,
        MPI_COMM_WORLD
    );


    // ========================================================
    // Timing file
    //
    // Only Rank 0 writes the file.
    // ========================================================

    if (rank == 0) {

        ofstream timing_out(timing_file);

        if (!timing_out) {

            cerr << "Error: could not open timing file: "
                 << timing_file << "\n";

            MPI_Type_free(&MPI_MEASUREMENT);
            MPI_Type_free(&MPI_EXTREME);

            MPI_Finalize();

            return 1;
        }


        double communication_time =
            distribution_time +
            aggregation_time;


        timing_out << fixed << setprecision(9);


        timing_out << "TIMING_TOTAL "
                   << total_time << "\n";


        timing_out << "TIMING_DISTRIBUTION "
                   << distribution_time << "\n";


        timing_out << "TIMING_COMPUTATION "
                   << computation_time << "\n";


        timing_out << "TIMING_AGGREGATION "
                   << aggregation_time << "\n";


        timing_out << "TIMING_FINAL_PROCESSING "
                   << final_processing_time << "\n";


        timing_out << "TIMING_COMMUNICATION "
                   << communication_time << "\n";


        timing_out.close();
    }


    // ========================================================
    // Cleanup
    // ========================================================

    MPI_Type_free(&MPI_MEASUREMENT);
    MPI_Type_free(&MPI_EXTREME);

    MPI_Finalize();

    return 0;
}