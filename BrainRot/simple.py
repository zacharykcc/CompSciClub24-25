def decode_string(encoded_text, seed_value):
    """Decodes the input encoded text using the same logic as encode_string."""
    decoded_string = ""
    
    for i, char in enumerate(encoded_text):
        # The seed value increments with each character
        seed_value += 1
        if seed_value > 26:
            seed_value = 0

        # For uppercase letters
        if 'A' <= char <= 'Z':
            new_char_value = ord(char) + seed_value
            if new_char_value > ord('Z'):
                new_char_value -= 26
            decoded_string += chr(new_char_value)

        # For lowercase letters
        elif 'a' <= char <= 'z':
            new_char_value = ord(char) + seed_value
            if new_char_value > ord('z'):
                new_char_value -= 26
            decoded_string += chr(new_char_value)

        # Non-alphabetic characters remain unchanged
        else:
            decoded_string += char
    
    return decoded_string

# Use the encoded string to find the original input
encoded_string = (
    "Gifcvse fb fhxo xtf htln dgx ewtsim xjy aw txra'e fiutf iwnbld qdz usx. "
    "Ggdlsppa B qq cao. lcugmd iok"
)

# Try to decode it with a seed value of 20
original_input = decode_string(encoded_string, 20)
print("Potential original input:", original_input)
