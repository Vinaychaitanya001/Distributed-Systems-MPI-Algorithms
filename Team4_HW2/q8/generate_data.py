import random
import argparse


def generate_dataset(N, K, S, output_file, seed=42):
    random.seed(seed)

    # Generate timestamps over a fixed time range.
    # Using 7 days gives us many different 60-second intervals.
    duration = 7 * 24 * 60 * 60

    with open(output_file, "w") as f:
        # First line: N K S
        f.write(f"{N} {K} {S}\n")

        for _ in range(N):
            # Integer timestamp
            timestamp = random.randint(0, duration - 1)

            # Station ID is in [0, S-1]
            station_id = random.randint(0, S - 1)

            # Weather values
            temperature = random.uniform(-10.0, 50.0)
            humidity = random.uniform(0.0, 100.0)
            pressure = random.uniform(950.0, 1050.0)
            rainfall = random.uniform(0.0, 20.0)
            wind_speed = random.uniform(0.0, 40.0)

            f.write(
                f"{timestamp} "
                f"{station_id} "
                f"{temperature:.6f} "
                f"{humidity:.6f} "
                f"{pressure:.6f} "
                f"{rainfall:.6f} "
                f"{wind_speed:.6f}\n"
            )


def main():
    parser = argparse.ArgumentParser(
        description="Generate reproducible Q8 weather datasets"
    )

    parser.add_argument("-n", "--N", type=int, required=True,
                        help="Number of measurements")
    parser.add_argument("-k", "--K", type=int, required=True,
                        help="Number of top stations")
    parser.add_argument("-s", "--S", type=int, required=True,
                        help="Number of weather stations")
    parser.add_argument("-o", "--output", type=str, default="weather.txt",
                        help="Output file")
    parser.add_argument("--seed", type=int, default=42,
                        help="Random seed")

    args = parser.parse_args()

    if args.N <= 0:
        raise ValueError("N must be positive")

    if args.K <= 0:
        raise ValueError("K must be positive")

    if args.S <= 0:
        raise ValueError("S must be positive")

    if args.K > args.S:
        raise ValueError("K cannot be greater than S")

    generate_dataset(
        args.N,
        args.K,
        args.S,
        args.output,
        args.seed
    )

    print(f"Generated {args.N} measurements")
    print(f"Stations: {args.S}")
    print(f"Top-K: {args.K}")
    print(f"Seed: {args.seed}")
    print(f"Output: {args.output}")


if __name__ == "__main__":
    main()