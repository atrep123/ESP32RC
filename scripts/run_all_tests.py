#!/usr/bin/env python3
"""
ESP32RC Comprehensive Test Suite Orchestrator

Runs all tests: sanity, unit, integration, and runtime simulation.
Provides unified reporting for CI/CD pipeline.
"""

import subprocess
import sys
from pathlib import Path


class TestOrchestrator:
    def __init__(self):
        self.project_dir = Path(__file__).parent.parent
        self.all_results = []
        self.total_passed = 0
        self.total_failed = 0

    def run_test_suite(self, suite_name, script_path):
        """Run a test suite and collect results"""
        print(f"\n{'='*70}")
        print(f"Running: {suite_name}")
        print(f"{'='*70}")
        
        result = subprocess.run(
            [sys.executable, str(script_path)],
            cwd=self.project_dir,
            capture_output=True,
            text=True
        )
        
        print(result.stdout)
        if result.stderr:
            print("STDERR:", result.stderr)
        
        # Extract results from output
        success = result.returncode == 0
        self.all_results.append({
            "suite": suite_name,
            "success": success,
            "output": result.stdout
        })
        
        # Try to parse passed/failed counts
        for line in result.stdout.split('\n'):
            if 'Results:' in line or 'Results:' in line:
                if 'passed' in line:
                    try:
                        parts = line.split()
                        for i, part in enumerate(parts):
                            if 'passed' in part and i > 0:
                                passed = int(parts[i-1])
                                self.total_passed += passed
                            if 'failed' in part and i > 0:
                                failed = int(parts[i-1])
                                self.total_failed += failed
                    except (ValueError, IndexError):
                        pass
        
        return success

    def print_summary(self):
        """Print comprehensive test summary"""
        print(f"\n{'='*70}")
        print("COMPREHENSIVE TEST SUMMARY")
        print(f"{'='*70}\n")
        
        print("Test Suites Run:")
        passed_suites = 0
        for result in self.all_results:
            status = "[PASS]" if result["success"] else "[FAIL]"
            print(f"  {status} {result['suite']}")
            if result["success"]:
                passed_suites += 1
        
        print(f"\nSuite Results: {passed_suites}/{len(self.all_results)} suites passed")
        print(f"Total Tests: {self.total_passed} passed, {self.total_failed} failed")
        
        print(f"\n{'='*70}")
        
        if self.total_failed == 0 and len(self.all_results) > 0:
            print("SUCCESS: ALL TESTS PASSED")
            return True
        else:
            print("FAILURE: SOME TESTS FAILED")
            return False

    def run_all_suites(self):
        """Execute all test suites in order"""
        scripts_dir = self.project_dir / "scripts"
        
        suites = [
            ("Build Sanity Tests (15 tests)", scripts_dir / "build_sanity_tests.py"),
            ("Unit Tests (16 tests)", scripts_dir / "unit_tests.py"),
            ("Integration Tests (16 tests)", scripts_dir / "integration_tests.py"),
            ("Runtime Simulator Tests (9 tests)", scripts_dir / "runtime_simulator_tests.py"),
        ]
        
        all_passed = True
        for suite_name, script_path in suites:
            if not script_path.exists():
                print(f"WARNING: {script_path} not found, skipping")
                continue
            
            success = self.run_test_suite(suite_name, script_path)
            if not success:
                all_passed = False
        
        return all_passed

    def main(self):
        """Main entry point"""
        print("ESP32RC Comprehensive Test Suite")
        print("=" * 70)
        print(f"Project Directory: {self.project_dir}")
        print(f"Running all available test suites...\n")
        
        all_passed = self.run_all_suites()
        success = self.print_summary()
        
        sys.exit(0 if success else 1)


if __name__ == "__main__":
    orchestrator = TestOrchestrator()
    orchestrator.main()
