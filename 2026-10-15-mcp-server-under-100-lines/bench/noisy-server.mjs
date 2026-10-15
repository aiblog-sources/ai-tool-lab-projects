// Stdio mistakes, on purpose, around the real server.
//   NOISY=start       one console.log line before the handshake
//   NOISY=each        one console.log line for every request (listener added after the transport)
//   NOISY=nonl        process.stdout.write("loading... ") with no newline, before the handshake
//   NOISY=early-stdin a stdin "data" listener added before the transport exists (this is how I first
//                     wrote NOISY=each; it puts stdin in flowing mode and the first request is lost)
const mode = process.env.NOISY;
if (mode === "start") console.log("notes-search starting");
if (mode === "nonl") process.stdout.write("loading... ");
if (mode === "early-stdin") process.stdin.on("data", () => console.log("got a request"));
await import("../server.mjs");
if (mode === "each") process.stdin.on("data", () => console.log("got a request"));
