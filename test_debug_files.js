// Test script to demonstrate debug file manager functionality

console.log("=== Testing Debug File Manager ===");

// Test 1: Direct eval
console.log("\n1. Testing direct eval:");
eval("console.log('Hello from eval!'); var x = 42;");

// Test 2: Eval with more complex code
console.log("\n2. Testing complex eval:");
eval(`
function testFunction() {
    console.log("This is a test function from eval");
    return 100;
}
var result = testFunction();
console.log("Result:", result);
`);

// Test 3: Dynamic function creation
console.log("\n3. Testing Function constructor:");
var dynamicFunc = new Function('a', 'b', 'return a + b;');
console.log("Dynamic function result:", dynamicFunc(10, 20));

// Test 4: Loading a module (if available)
console.log("\n4. Testing module loading:");
try {
    import('./simple_test.js').then(module => {
        console.log("Module loaded successfully");
    }).catch(err => {
        console.log("Module loading failed:", err);
    });
} catch (e) {
    console.log("Import not available in this context");
}

// Test 5: Multiple evals to test counter
console.log("\n5. Testing multiple evals:");
for (let i = 0; i < 5; i++) {
    eval(`console.log('Eval #${i + 1}');`);
}

console.log("\n=== Test Complete ===");
console.log("Check the 'debug_sources' directory for generated files!");
