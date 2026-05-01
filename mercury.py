#!/usr/bin/env python3

import asyncio
import json
import csv
import sys
import os
import time
import random
import argparse
from datetime import datetime
from pathlib import Path

try:
    import aiohttp
    from colorama import Fore, Back, Style, init
    init(autoreset=True)
    HAS_DEPS = True
except ImportError:
    HAS_DEPS = False

BANNER = r"""
  ___  ___                               _____ _____ _____ _   _ _____ 
  |  \/  |                              |  _  /  ___|_   _| \ | |_   _|
  | .  . | ___ _ __ ___ _   _ _ __ _   _| | | \ `--.  | | |  \| | | |  
  | |\/| |/ _ \ '__/ __| | | | '__| | | | | | |`--. \ | | | . ` | | |  
  | |  | |  __/ | | (__| |_| | |  | |_| \ \_/ /\__/ /_| |_| |\  | | |  
  \_|  |_/\___|_|  \___|\__,_|_|   \__, |\___/\____/ \___/\_| \_/ \_/  
                                    __/ |                                
                                   |___/                                 
"""

TAGLINE = "  MercuryOSINT v1.0  |  Ethical Use Only  |  1500+ Platforms"

ALIASES = [
    "Anonymous", "Ghost", "Phantom", "Shadow", "Cipher",
    "Spectre", "Wraith", "Veil", "Echo", "Null"
]

class C:
    if HAS_DEPS:
        CYAN    = Fore.CYAN
        GREEN   = Fore.GREEN
        RED     = Fore.RED
        YELLOW  = Fore.YELLOW
        MAGENTA = Fore.MAGENTA
        WHITE   = Fore.WHITE
        GRAY    = Fore.WHITE + Style.DIM
        BOLD    = Style.BRIGHT
        RESET   = Style.RESET_ALL
        DIM     = Style.DIM
    else:
        CYAN = GREEN = RED = YELLOW = MAGENTA = WHITE = GRAY = BOLD = RESET = DIM = ""


def clr(text, color):
    return f"{color}{text}{C.RESET}"


def load_sites():
    db_path = Path(__file__).parent / "data" / "sites.json"
    if not db_path.exists():
        print(clr("[!] sites.json not found. Run from the mercury tool directory.", C.RED))
        sys.exit(1)
    with open(db_path) as f:
        return json.load(f)


HEADERS = {
    "User-Agent": (
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) "
        "AppleWebKit/537.36 (KHTML, like Gecko) "
        "Chrome/124.0.0.0 Safari/537.36"
    ),
    "Accept-Language": "en-US,en;q=0.9",
}

async def check_site(session, name, data, username, semaphore, results, verbose=False):
    url_template = data.get("url", "")
    error_type   = data.get("error_type", "status_code")
    error_code   = data.get("errorCode", 404)
    error_msg    = data.get("errorMsg", "")

    url = url_template.format(username) if "{}" in url_template else url_template.replace("{username}", username)

    async with semaphore:
        try:
            timeout = aiohttp.ClientTimeout(total=10)
            async with session.get(url, timeout=timeout, allow_redirects=True, ssl=False) as resp:
                status = resp.status

                if error_type == "status_code":
                    found = (status != error_code and status not in [400, 401, 403, 429, 500, 502, 503])
                elif error_type == "message":
                    text = await resp.text(errors="ignore")
                    found = error_msg not in text
                else:
                    found = status == 200

                result = {
                    "site":   name,
                    "url":    url,
                    "status": status,
                    "found":  found,
                }
                results.append(result)

                if found:
                    print(f"  {clr('+', C.GREEN)} {clr(name.ljust(30), C.BOLD)} {clr(url, C.CYAN)}")
                elif verbose:
                    print(f"  {clr('-', C.GRAY)} {clr(name.ljust(30), C.GRAY)} {clr(str(status), C.GRAY)}")

        except asyncio.TimeoutError:
            results.append({"site": name, "url": url, "status": "TIMEOUT", "found": False})
            if verbose:
                print(f"  {clr('~', C.YELLOW)} {clr(name.ljust(30), C.GRAY)} {clr('TIMEOUT', C.YELLOW)}")
        except Exception as e:
            results.append({"site": name, "url": url, "status": "ERROR", "found": False})
            if verbose:
                print(f"  {clr('!', C.RED)} {clr(name.ljust(30), C.GRAY)} {clr(str(e)[:40], C.RED)}")


async def progress_ticker(total, results, done_event):
    start = time.time()
    spinner = [".", "o", "O", "o"]
    i = 0
    while not done_event.is_set():
        checked = len(results)
        found   = sum(1 for r in results if r["found"])
        elapsed = time.time() - start
        pct     = (checked / total * 100) if total else 0
        spin    = spinner[i % len(spinner)]
        i += 1
        bar_len = 30
        filled  = int(bar_len * pct / 100)
        bar     = clr("#" * filled, C.GREEN) + clr("." * (bar_len - filled), C.GRAY)
        line = (
            f"\r  {clr(spin, C.CYAN)} [{bar}] "
            f"{clr(f'{pct:.1f}%', C.BOLD)} "
            f"{clr(f'{checked}/{total}', C.WHITE)} checked  "
            f"{clr(f'+ {found}', C.GREEN)} found  "
            f"{clr(f'{elapsed:.1f}s', C.YELLOW)}   "
        )
        print(line, end="", flush=True)
        await asyncio.sleep(0.1)
    print()


def export_results(username, results, output_dir):
    found = [r for r in results if r["found"]]
    ts    = datetime.now().strftime("%Y%m%d_%H%M%S")
    out   = Path(output_dir)
    out.mkdir(parents=True, exist_ok=True)

    json_path = out / f"mercury_{username}_{ts}.json"
    with open(json_path, "w") as f:
        json.dump({"username": username, "timestamp": ts, "results": found}, f, indent=2)

    csv_path = out / f"mercury_{username}_{ts}.csv"
    with open(csv_path, "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=["site", "url", "status"])
        w.writeheader()
        for r in found:
            w.writerow({"site": r["site"], "url": r["url"], "status": r["status"]})

    txt_path = out / f"mercury_{username}_{ts}.txt"
    with open(txt_path, "w") as f:
        f.write(f"MercuryOSINT Report\n")
        f.write(f"Username : {username}\n")
        f.write(f"Date     : {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n")
        f.write(f"Found    : {len(found)} profiles\n")
        f.write("=" * 60 + "\n\n")
        for r in found:
            f.write(f"[{r['site']}]\n  {r['url']}\n\n")

    return json_path, csv_path, txt_path


async def run_scan(username, sites, concurrency=50, verbose=False, output_dir="output"):
    results = []
    done_event = asyncio.Event()
    total = len(sites)

    print(f"\n  {clr('TARGET', C.MAGENTA + C.BOLD)} : {clr(username, C.CYAN + C.BOLD)}")
    print(f"  {clr('SITES ', C.MAGENTA + C.BOLD)} : {clr(str(total), C.WHITE)}")
    print(f"  {clr('CONC. ', C.MAGENTA + C.BOLD)} : {clr(str(concurrency), C.WHITE)} simultaneous requests")
    print(f"\n  {clr('-' * 60, C.GRAY)}")
    print(f"  {clr('LIVE HITS', C.GREEN + C.BOLD)}\n")

    semaphore = asyncio.Semaphore(concurrency)

    connector = aiohttp.TCPConnector(limit=concurrency, ssl=False)
    async with aiohttp.ClientSession(headers=HEADERS, connector=connector) as session:
        ticker_task = asyncio.create_task(progress_ticker(total, results, done_event))
        tasks = [
            check_site(session, name, data, username, semaphore, results, verbose)
            for name, data in sites.items()
        ]
        await asyncio.gather(*tasks)
        done_event.set()
        await ticker_task

    found = [r for r in results if r["found"]]
    print(f"\n  {clr('-' * 60, C.GRAY)}")
    print(f"\n  {clr('SCAN COMPLETE', C.BOLD + C.CYAN)}")
    print(f"  {clr('+ Found', C.GREEN)} : {clr(str(len(found)), C.BOLD + C.GREEN)} profiles")
    print(f"  {clr('- Not found / error', C.GRAY)} : {clr(str(total - len(found)), C.GRAY)}")

    if found:
        j, c, t = export_results(username, results, output_dir)
        print(f"\n  {clr('OUTPUT', C.MAGENTA + C.BOLD)}")
        print(f"  {clr('JSON', C.CYAN)} -> {j}")
        print(f"  {clr('CSV ', C.CYAN)} -> {c}")
        print(f"  {clr('TXT ', C.CYAN)} -> {t}")

    return results


def print_banner():
    alias = random.choice(ALIASES)
    if HAS_DEPS:
        print(clr(BANNER, C.CYAN))
        print(clr(TAGLINE, C.MAGENTA + C.BOLD))
        print()
        print(f"  {clr('Welcome to Mercury,', C.WHITE)} {clr(alias, C.CYAN + C.BOLD)}{clr('.', C.WHITE)}")
        print(f"  {clr('Here you will use this tool, MercuryOSINT for ethical uses.', C.GRAY)}")
        print()
        print(f"  {clr('This tool is for LEGAL, ETHICAL research only.', C.YELLOW)}")
        print(f"  {clr('Unauthorized use may violate laws. You are responsible.', C.YELLOW)}")
    else:
        print(BANNER)
        print(TAGLINE)
        print(f"\nWelcome to Mercury, {alias}.")
        print("MercuryOSINT - Ethical use only.\n")
    print()


def parse_args():
    parser = argparse.ArgumentParser(
        prog="mercury",
        description="MercuryOSINT - Username OSINT across 1500+ platforms",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python3 mercury.py johndoe
  python3 mercury.py johndoe --concurrency 100 --verbose
  python3 mercury.py johndoe --output ./results
  python3 mercury.py johndoe --filter gaming
        """
    )
    parser.add_argument("username", nargs="?", help="Username to search")
    parser.add_argument("-c", "--concurrency", type=int, default=50,
                        help="Concurrent requests (default: 50)")
    parser.add_argument("-v", "--verbose", action="store_true",
                        help="Show all results including misses")
    parser.add_argument("-o", "--output", default="output",
                        help="Output directory (default: ./output)")
    parser.add_argument("--filter", metavar="CATEGORY",
                        help="Filter sites by name keyword (e.g. 'github', 'gaming')")
    parser.add_argument("--list-sites", action="store_true",
                        help="List all sites in the database and exit")
    parser.add_argument("--fast", action="store_true",
                        help="Fast mode: top 200 most popular sites only")
    return parser.parse_args()


def main():
    print_banner()

    if not HAS_DEPS:
        print("[!] Missing dependencies. Install with:")
        print("    pip install aiohttp colorama")
        sys.exit(1)

    args = parse_args()
    sites = load_sites()

    if args.list_sites:
        print(clr(f"  {'SITE NAME':<40} {'URL TEMPLATE'}", C.BOLD))
        print(clr(f"  {'-'*40} {'-'*40}", C.GRAY))
        for name, data in sorted(sites.items()):
            print(f"  {clr(name.ljust(40), C.CYAN)} {clr(data['url'], C.GRAY)}")
        print(f"\n  Total: {len(sites)} sites")
        return

    if args.filter:
        keyword = args.filter.lower()
        sites = {k: v for k, v in sites.items() if keyword in k.lower() or keyword in v.get("url","").lower()}
        print(clr(f"  [filter] Matched {len(sites)} sites for '{args.filter}'", C.YELLOW))

    if args.fast:
        popular = list(sites.items())[:200]
        sites = dict(popular)
        print(clr(f"  [fast] Scanning top 200 sites only", C.YELLOW))

    username = args.username
    if not username:
        print(clr("  Enter username to search:", C.BOLD))
        username = input("  > ").strip()
        if not username:
            print(clr("  No username provided. Exiting.", C.RED))
            sys.exit(1)

    if len(username) < 1 or len(username) > 64:
        print(clr("  [!] Username must be 1-64 characters.", C.RED))
        sys.exit(1)

    asyncio.run(run_scan(
        username=username,
        sites=sites,
        concurrency=args.concurrency,
        verbose=args.verbose,
        output_dir=args.output,
    ))

    print()
    print(clr("Stay Curious. Stay Anonymous.", C.GRAY))
    print()


if __name__ == "__main__":
    main()
