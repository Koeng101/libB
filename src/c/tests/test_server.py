#!/usr/bin/env python3
"""
Test suite for the Lua Sandbox HTTP Server.
Uses unittest framework to properly validate server responses.
"""

import unittest
import requests
import json
import time
import sys

SERVER_URL = "http://localhost:8080"

class LuaSandboxServerTests(unittest.TestCase):
    """Test cases for the Lua Sandbox HTTP Server"""

    def send_request(self, code, config=None):
        """Helper method to send a request to the server"""
        payload = {"code": code}
        if config:
            payload["config"] = config
        
        response = requests.post(SERVER_URL, json=payload, timeout=10)
        
        return response
    
    def test_basic_arithmetic(self):
        """Test basic arithmetic operations"""
        response = self.send_request("return 2 + 2 * 10")
        self.assertEqual(response.status_code, 200)
        
        result = response.json()
        self.assertEqual(result["status"], 0)
        self.assertEqual(result["output"], "22")
    
    def test_string_manipulation(self):
        """Test string manipulation functions"""
        response = self.send_request("return string.upper('hello world')")
        self.assertEqual(response.status_code, 200)
        
        result = response.json()
        self.assertEqual(result["status"], 0)
        self.assertEqual(result["output"], "HELLO WORLD")
    
    def test_table_to_json(self):
        """Test table to JSON conversion"""
        response = self.send_request(
            "return {name='test', values={1,2,3,4}, nested={a=1, b=2}}"
        )
        self.assertEqual(response.status_code, 200)
        
        result = response.json()
        self.assertEqual(result["status"], 0)
        
        # Parse the output as JSON to verify structure
        output = json.loads(result["output"])
        self.assertEqual(output["name"], "test")
        self.assertEqual(output["values"], [1, 2, 3, 4])
        self.assertEqual(output["nested"], {"a": 1, "b": 2})
    
    def test_debug_output(self):
        """Test debug output capture"""
        response = self.send_request(
            "print('Debug message 1')\nprint('Debug message 2')\nreturn 'done'"
        )
        self.assertEqual(response.status_code, 200)
        
        result = response.json()
        self.assertEqual(result["status"], 0)
        self.assertEqual(result["output"], "done")
        self.assertIn("Debug message 1", result["debug"])
        self.assertIn("Debug message 2", result["debug"])
    
    def test_dna_design_module(self):
        """Test the DNA design module"""
        response = self.send_request(
            "return dnadesign.transform.reverse_complement('GATTACA')"
        )
        self.assertEqual(response.status_code, 200)
        
        result = response.json()
        self.assertEqual(result["status"], 0)
        self.assertEqual(result["output"], "TGTAATC")
    
    def test_syntax_error(self):
        """Test handling of syntax errors"""
        response = self.send_request("return 1 + + 2")
        self.assertEqual(response.status_code, 200)
        
        result = response.json()
        self.assertTrue(
            "unexpected symbol" in result["output"].lower() or
            "[string" in result["output"].lower()
        )
        self.assertEqual(result["status"], 1)  # Error status
    
    def test_custom_memory_limit(self):
        """Test custom memory limit enforcement"""
        # This should fail with a very low memory limit
        response = self.send_request(
            "local t = {}\nfor i=1,100000 do t[i] = string.rep('x', 100) end\nreturn #t",
            {"memory_limit": 100000}  # Very small limit
        )
        self.assertEqual(response.status_code, 200)
        
        result = response.json()
        self.assertEqual(result["status"], 1)  # Error status
        # Look for memory-related error messages, content may vary
        self.assertTrue(
            "memory" in result["output"].lower() or 
            "allocation" in result["output"].lower() or
            "not enough memory" in result["output"].lower()
        )
    
    def test_default_memory_limit(self):
        """Test that the default memory limit is sufficient for reasonable code"""
        response = self.send_request(
            "local t = {}\nfor i=1,1000 do t[i] = string.rep('x', 100) end\nreturn #t"
        )
        self.assertEqual(response.status_code, 200)
        
        result = response.json()
        self.assertEqual(result["status"], 0)
        self.assertEqual(result["output"], "1000")
    
    def test_invalid_json_request(self):
        """Test handling of invalid JSON requests"""
        response = requests.post(SERVER_URL, data="Not a JSON", timeout=10)
        self.assertEqual(response.status_code, 400)
        
        # Response should be JSON with an error message
        result = response.json()
        self.assertIn("error", result)
    
    def test_missing_code_field(self):
        """Test handling of requests missing the 'code' field"""
        response = requests.post(SERVER_URL, json={"not_code": "return 1"}, timeout=10)
        self.assertEqual(response.status_code, 400)
        
        # Response should be JSON with an error message
        result = response.json()
        self.assertIn("error", result)
        self.assertIn("code", result["error"].lower())
    
    def test_performance(self):
        """Test server performance with a simple benchmark"""
        start_time = time.time()
        iterations = 10
        
        for _ in range(iterations):
            response = self.send_request("return 42")
            self.assertEqual(response.status_code, 200)
        
        elapsed = time.time() - start_time
        avg_time = elapsed / iterations
        
        # This is not a strict test but useful information
        print(f"\nPerformance: {iterations} requests in {elapsed:.3f}s (avg: {avg_time:.3f}s per request)")
        # Soft threshold - this is informational, not a strict requirement
        self.assertLess(avg_time, 0.5, "Average request time exceeds 500ms threshold")

def check_server_availability():
    """Check if the server is running before running tests"""
    try:
        requests.get(SERVER_URL, timeout=2)
        return True
    except requests.RequestException:
        return False

if __name__ == "__main__":
    # Check if server is available before running tests
    if not check_server_availability():
        print("Error: Server is not responding. Make sure it's running at", SERVER_URL)
        sys.exit(1)
    
    print(f"Testing Lua Sandbox Server at {SERVER_URL}...")
    unittest.main()
