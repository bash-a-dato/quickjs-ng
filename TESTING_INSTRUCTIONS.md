# Testing the QuickJS Debugger File Manager

## Build with Debugger Enabled

Now you can build QuickJS with the debugger file manager enabled using:

```powershell
# Option 1: Build in Debug mode (automatically enables debugger)
.\build.ps1 -Debug

# Option 2: Build in Release mode with debugger enabled
.\build.ps1 -Debugger

# Option 3: Clean rebuild with debugger
.\build.ps1 -Rebuild -Debug
```

## Verify the Build

After building, you should see in the CMake output:
```
-- CONFIG_DEBUGGER: ON
```

And in the build script output:
```
Debugger: Enabled (with file manager)
```

## Test the Debugger File Manager

1. **Run the simple test:**
   ```powershell
   .\build\Debug\qjs.exe test_debugger_simple.js
   ```

2. **Check for generated debug files:**
   After running the test, check if a `debug_sources` directory was created with files like:
   - `0001_test_debugger_simple.js` (the main script)
   - `0002_<eval_1>.js` (first eval call)
   - `0003_<eval_2>.js` (second eval call) 
   - `0004_<eval_3>.js` (third eval call)

3. **Run the comprehensive test:**
   ```powershell
   .\build\Debug\qjs.exe test_debug_files.js
   ```

## Expected Output

If the debugger is working correctly, you should see output like:
```
=== QuickJS Debugger File Manager Test ===

1. Testing simple eval:
[DEBUG] Registered file: <eval_1> -> debug_sources\0002_<eval_1>.js (ID: 2, eval, script)
Hello from eval 1!

2. Testing another eval:
[DEBUG] Registered file: <eval_2> -> debug_sources\0003_<eval_2>.js (ID: 3, eval, script)
Variable from eval: 42

3. Testing function in eval:
[DEBUG] Registered file: <eval_3> -> debug_sources\0004_<eval_3>.js (ID: 4, eval, script)
Function from eval executed!
Function result: success

=== Test Complete ===
```

## Troubleshooting

If you don't see the `[DEBUG]` messages or the `debug_sources` directory:

1. **Check if CONFIG_DEBUGGER is defined:**
   ```powershell
   # Compile and run the config test
   cl test_config.c /DCONFIG_DEBUGGER
   .\test_config.exe
   ```
   Should output: `CONFIG_DEBUGGER is ENABLED`

2. **Verify the build included the file manager:**
   Check that `quickjs-debugger-files-manager.c` was compiled without errors.

3. **Check CMake configuration:**
   Look for `CONFIG_DEBUGGER: ON` in the CMake output.

## File Structure

When working correctly, you'll see:
```
debug_sources/
├── 0001_test_debugger_simple.js
├── 0002_<eval_1>.js  
├── 0003_<eval_2>.js
└── 0004_<eval_3>.js
```

Each file contains the actual JavaScript code that was executed, allowing you to debug it as if it were a regular source file.
