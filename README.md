# Mercury OSINT
Fast, lightweight username intelligence across 1500+ platforms.


OVERVIEW
MercuryOSINT is a dual-engine OSINT tool made by homueicon and the 
mercury-osint-project that checks for username presence
across hundreds of social networks, developer platforms, forums, and other
online services. It ships with both a high-performance C probe for speed
and a full-featured Python engine for comprehensive scanning.


FEATURES
- 1500+ supported platforms (full Python database)
- Dual engine: C (probing) + Python (comprehensive)
- Multi-threaded / async concurrent checks
- Live progress display
- Multiple export formats (JSON, CSV, TXT)
- Category filtering and fast mode
- Minimal dependencies


INSTALLATION
```$ git clone https://github.com/mercury-osint-project/MercuryOSINT.git
```
```$ cd mercuryosint
```
```
$ pip install -r requirements.txt
```

Dependencies
- aiohttp>=3.8
- colorama>=0.4

Optional: Build C Probe
$ gcc -O2 -o mercury_probe mercury_probe.c -lpthread

For HTTPS support:
$ gcc -O2 -o mercury_probe mercury_probe.c -lpthread -lssl -lcrypto -DWITH_SSL


USAGE
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


OPTIONS
Flag                 Description
username             Target username
-c, --concurrency    Concurrent requests (default: 50)
-v, --verbose        Show all results including misses
-o, --output         Output directory (default: ./output)
--filter             Filter by name keyword
--fast               Top 200 most popular sites only
--list-sites         List all sites in database


OUTPUT
Results are saved to the output/ directory in three formats:

output/
  mercury_johndoe_20260101_120000.json
  mercury_johndoe_20260101_120000.csv
  mercury_johndoe_20260101_120000.txt


PROJECT STRUCTURE
mercuryosint/
  mercury.py           Python engine (main)
  mercury_probe.c      C probe companion
  data/
    sites.json         Site database
  output/              Scan results


DISCLAIMER
This tool is intended for legal, ethical research purposes only.
Unauthorized use against systems you do not own or have explicit
permission to test is prohibited. Users are solely responsible for
compliance with applicable laws.


LICENSE
MIT
