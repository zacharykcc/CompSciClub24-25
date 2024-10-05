import subprocess
import time
import re

def main():
    combos = ['a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', "!", "@", "#", "$", "%", "^", "&", "*", "(", ")", "_", "-", "=", "+", "[", "]", "{", "}", "|", ";", ":", "'", "<", ">", ",", ".", "?", "/", "~", "`", "\\", "\"", " "]
    
    previous_match_count = None
    
    with subprocess.Popen(['nc', 'ctf.mwales.net', '45040'], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True) as proc:
        for combo in combos:
            time.sleep(0.1)  # Adjust this if needed
            proc.stdin.write(f"wildcat{{${{combo}}}\n}")  # Using f-string with escaped braces
            proc.stdin.flush()
            
            # Read the response from nc
            response = proc.stdout.readline().strip()
            
            # Check for "X characters matched" in the response
            match = re.search(r'(\d+) characters matched', response)
            if match:
                matched_count = int(match.group(1))
                
                if matched_count != previous_match_count:
                    print(f"Attempt: wildcat{{{combo}}}")
                    print(f"Response: {response}")
                    previous_match_count = matched_count

if __name__ == "__main__":
    main()
