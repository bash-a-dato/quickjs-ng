console.log("Starting QuickJS debugging example...");

function fibonacci(n) {
    if (n <= 1) {
        return n;
    }
    return fibonacci(n - 1) + fibonacci(n - 2);
}

function main() {
    console.log("Computing fibonacci numbers:");
    
    for (let i = 0; i < 10; i++) {
        const result = fibonacci(i);
        console.log(`fibonacci(${i}) = ${result}`);
    }
    
     console.log("Done!");
}

// Set breakpoint here to debug
main();