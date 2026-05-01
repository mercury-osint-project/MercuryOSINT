# Mercury OSINT

Fast, lightweight username intelligence across 1500+ platforms.


## Overview

MercuryOSINT is a dual-engine OSINT tool built by
[homueicon](https://github.com/homueicon) and the
[mercury-osint-project](https://github.com/mercury-osint-project)
that checks for username presence across hundreds of social networks,
developer platforms, forums, and other online services. It ships with
both a high-performance C probe for speed and a full-featured Python
engine for comprehensive scanning.


## Features

- 1500+ supported platforms with full Python database
- Dual-engine architecture
  - C probe: Maximum throughput with minimal overhead
  - Python engine: Comprehensive scanning with async I/O
- Multi-threaded / async concurrent checks
- Live progress display with real-time statistics
- Multiple export formats (JSON, CSV, TXT)
- Category filtering and fast mode
- Minimal dependencies (aiohttp + colorama)


## Installation

Clone the repository:

  $ git clone https://github.com/mercury-osint-project/MercuryOSINT.git
  $ cd MercuryOSINT

Install Python dependencies:

  $ pip install -r requirements.txt

Dependencies

  - [aiohttp](https://pypi.org/project/aiohttp/) >= 3.8
  - [colorama](https://pypi.org/project/colorama/) >= 0.4

Optional: Build C Probe

  $ gcc -O2 -o mercury_probe mercury_probe.c -lpthread

For HTTPS support:

  $ gcc -O2 -o mercury_probe mercury_probe.c -lpthread -lssl -lcrypto -DWITH_SSL


## Usage

Python Engine

  $ python3 mercury.py johndoe
  $ python3 mercury.py johndoe --concurrency 100 --verbose
  $ python3 mercury.py johndoe --output ./results
  $ python3 mercury.py johndoe --filter gaming
  $ python3 mercury.py johndoe --fast

C Probe

  $ ./mercury_probe johndoe
  $ ./mercury_probe johndoe 100
  $ cat urls.txt | ./mercury_probe johndoe


## Options

  Flag                  Description
  -----------------------------------------------
  username              Target username to search
  -c, --concurrency     Concurrent requests (default: 50)
  -v, --verbose         Show all results including misses
  -o, --output          Output directory (default: ./output)
  --filter CATEGORY     Filter sites by name keyword (e.g. 'github', 'gaming')
  --fast                Top 200 most popular sites only
  --list-sites          List all sites in database and exit


## Output

Results are saved to the output/ directory in three formats:

  output/
    mercury_johndoe_20260101_120000.json
    mercury_johndoe_20260101_120000.csv
    mercury_johndoe_20260101_120000.txt

JSON format includes full metadata. CSV is spreadsheet-ready.
TXT is human-readable with URLs grouped by site.


## Project Structure

  MercuryOSINT/
    mercury.py           Python engine (main scanner)
    mercury_probe.c      C probe companion
    requirements.txt     Python dependencies
    data/
      sites.json         Site database (1500+ platforms)
    output/              Scan results directory


## Disclaimer

This tool is intended for legal, ethical research purposes only.
Unauthorized use against systems you do not own or have explicit
permission to test is prohibited. Users are solely responsible for
compliance with applicable laws in their jurisdiction.


## License

[MIT](https://github.com/mercury-osint-project/MercuryOSINT/blob/main/LICENSE)


## Credits

Created by [homueicon](https://github.com/homueicon)
Maintained by the [mercury-osint-project](https://github.com/mercury-osint-project)
