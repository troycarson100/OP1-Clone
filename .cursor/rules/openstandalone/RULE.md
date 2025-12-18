---
alwaysApply: true
---

After every update I ask you to make, ALWAYS close and reopen the standalone app.

**MANDATORY WORKFLOW:**
1. Before making any code changes, kill any running Op1Clone process
2. Make the code changes
3. Build the project
4. Kill the Op1Clone process again (if it's running)
5. Open the standalone app
6. Bring it to the front using osascript

**NEVER skip opening the app after an update unless explicitly told not to.**