#!/usr/bin/env python3

import re

# Read the file
with open('server_with_auth.cpp', 'r') as f:
    content = f.read()

# Pattern to match the hardcoded user_id assignment
pattern = r'std::string user_id = "authenticated_user"; // TODO: Extract from JWT'

# Replacement that adds authentication check
replacement = '''std::string user_id = authenticate_and_get_user_id(headers);
                    if (user_id.empty()) {
                        response = "{\"error\":\"Unauthorized: Invalid or missing authentication token\"}";
                    } else {
                        // Continue with original logic
                        response ='''

# Replace all occurrences
new_content = content.replace(pattern, replacement)

# But this replacement needs to be more context-aware. Let me check what comes after the user_id assignment
# and adjust the replacement accordingly.

# Actually, let me use a more sophisticated approach with regex
def replace_user_id(match):
    # Get the full match and some context
    full_match = match.group(0)
    # Find what comes after this line
    start_pos = match.end()
    # Look for the next response = assignment
    next_response = content.find('response =', start_pos)
    if next_response != -1:
        # Extract the response assignment
        response_end = content.find(';', next_response)
        if response_end != -1:
            response_assignment = content[next_response:response_end+1]
            # Create the replacement
            replacement = f'''std::string user_id = authenticate_and_get_user_id(headers);
                    if (user_id.empty()) {{
                        response = "{{\"error\":\"Unauthorized: Invalid or missing authentication token\"}}";
                    }} else {{
                        {response_assignment}
                    }}'''
            return replacement
    return full_match

# Use regex to find and replace
new_content = re.sub(pattern, replace_user_id, content)

# Write back
with open('server_with_auth.cpp', 'w') as f:
    f.write(new_content)

print("Replacement completed")
