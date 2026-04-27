class SymbolTable {
    constructor(symbols) {
        this.symbols = symbols;
    }

    static deserialize(view, offset) {
        const numSymbols = view.getUint8(offset);
        offset += 1;
        const symbols = [];
        const decoder = new TextDecoder();
        for (let i = 0; i < numSymbols; i++) {
            const currentLength = view.getUint8(offset);
            offset += 1;
            const symbolData = new Uint8Array(view.buffer, offset, currentLength);
            symbols.push(decoder.decode(symbolData));
            offset += currentLength;
        }
        return { table: new SymbolTable(symbols), newOffset: offset };
    }
}

class State {
    constructor(name, level, stateptr, transitions, endIdx, defaultTransition = null) {
        this.name = name;
        this.level = level;
        this.stateptr = stateptr;
        this.transitions = transitions;
        this.defaultTransition = defaultTransition;
        this.endIdx = endIdx;
    }
}

class StateDeserializer {
    static readUint64(view, offset) {
        return { val: view.getBigUint64(offset, true), next: offset + 8 };
    }

    static deserializeWithoutTransitions(view, offset, name, defaultVal) {
        let { val: stateptr, next: off } = this.readUint64(view, offset);
        let { val: level, next: off2 } = this.readUint64(view, off);
        const endIdx = view.getUint8(off2);
        return {
            state: new State(name, Number(level), stateptr, {}, Number(endIdx), defaultVal),
            newOffset: off2 + 1
        };
    }

    static deserializeWithoutDefaultTransition(view, offset, name, defaultVal) {
        let { state, newOffset: off } = this.deserializeWithoutTransitions(view, offset, name, defaultVal);
        let { val: numTransitions, next: off2 } = this.readUint64(view, off);
        off = off2;

        for (let i = 0; i < Number(numTransitions); i++) {
            const symbol = view.getUint8(off);
            off += 1;
            let { val: destptr, next: offNext } = this.readUint64(view, off);
            state.transitions[symbol] = destptr;
            off = offNext;
        }
        return { state: state, newOffset: off };
    }

    static deserializeFull(view, offset, name) {
        let { state: state, newOffset: off } = this.deserializeWithoutDefaultTransition(view, offset, name, null);
        let { val: defaultPtr, next: off2 } = this.readUint64(view, off);
        state.defaultTransition = defaultPtr;
        return { state: state, newOffset: off2 };
    }
}

class Automaton {
    constructor(states, errorState, acceptStates, subMapping = {}) {
        this.states = states;
        this.errorState = errorState;
        this.acceptStates = acceptStates;
        this.subMapping = subMapping;
        this.drawnStates = new Set();
        this.escapedForward = new Set();
        this.escapedBackward = new Set();
        this.pseudoEnds = new Set();
    }

    getAllStates() {
        return [...this.states, ...this.acceptStates, this.errorState];
    }

    getDrawnStates(starts) {
        starts.forEach(state => {
            console.log(state.name);
            if (Object.keys(state.transitions).length > 0) {
                this.drawnStates.add(state.name);
                for (const [symbol, destName] of Object.entries(state.transitions)) {
                    this.drawnStates.add(destName);
                }
            }
        })
        this.states.forEach(state => {
            for (const [symbol, destName] of Object.entries(state.transitions)) {
                this.drawnStates.add(destName);
            }
        })
    }

    bfs(starts, forward) {
        const mapping = {};
        this.getAllStates().forEach(n => mapping[n.name] = n);
        mapping['ERR'] = this.errorState;

        const queue = [...starts];
        const visited = new Set(starts);

        while (queue.length > 0) {
            const elem = queue.shift();
            const state = mapping[elem];
            if (!state) continue;

            for (const [symbol, destName] of Object.entries(state.transitions)) {
                if (parseInt(symbol) === 255) {
                    if (forward) this.escapedForward.add(destName);
                    else this.escapedBackward.add(elem);
                }
                if (!visited.has(destName)) {
                    visited.add(destName);
                    queue.push(destName);
                }
            }
            if (state.defaultTransition && !visited.has(state.defaultTransition)) {
                visited.add(state.defaultTransition);
                queue.push(state.defaultTransition);
            }
        }
        return visited;
    }

    eraseRedundant(visited) {
        this.states = this.states.filter(n => visited.has(n.name));
        this.acceptStates = this.acceptStates.filter(n => visited.has(n.name));
    }
}

class AutomatonDeserializer {
    /**
     * Helper to perform pointer-to-name translation for a list of states
     */
    static translatePointers(states, translations) {
        for (let state of states) {
            // Translate transitions
            for (let symbol in state.transitions) {
                const destPtr = state.transitions[symbol];
                state.transitions[symbol] = translations[destPtr];
            }
            // Translate default transition
            if (state.defaultTransition !== null && translations[state.defaultTransition]) {
                state.defaultTransition = translations[state.defaultTransition];
            }
        }
    }

    static deserializeSingleStart(view, forward) {
        let offset = 0;
        const { table: symbolTable, newOffset: off1 } = SymbolTable.deserialize(view, offset);
        offset = off1;

        const translations = {};
        const states = [];
        const starts = ["S"];
        const startStates = []

        // 1. Deserialize Start State 'S'
        const { state: startState, newOffset: off2 } = StateDeserializer.deserializeFull(view, offset, 'S');
        states.push(startState);
        startStates.push(startState);
        translations[startState.stateptr] = "S";
        offset = off2;

        // 2. Deserialize Error State
        const { state: errorState, newOffset: off3 } = StateDeserializer.deserializeWithoutTransitions(view, offset, 'ERR', 'ERR');
        translations[errorState.stateptr] = "ERR";
        offset = off3;

        // 3. Deserialize Accept States
        const acceptStates = [];
        const { val: numAccepts, next: off4 } = StateDeserializer.readUint64(view, offset);
        offset = off4;
        for (let i = 0; i < Number(numAccepts); i++) {
            const { state: acc, newOffset: offNext } = StateDeserializer.deserializeWithoutTransitions(view, offset, `E${i}`, 'ERR');
            acc.defaultTransition = null;
            acceptStates.push(acc);
            translations[acc.stateptr] = acc.name;
            offset = offNext;
        }

        // 4. Deserialize Intermediate States
        const { val: numStates, next: off5 } = StateDeserializer.readUint64(view, offset);
        offset = off5;
        for (let i = 0; i < Number(numStates); i++) {
            const { state: state, newOffset: offNext } = StateDeserializer.deserializeFull(view, offset, `q${i}`);
            states.push(state);
            translations[state.stateptr] = state.name;
            offset = offNext;
        }

        // 5. Finalize: Translate pointers and prune
        this.translatePointers(states, translations);
        const automaton = new Automaton(states, errorState, acceptStates);
        const visited = automaton.bfs(starts, forward);
        automaton.eraseRedundant(visited);
        automaton.getDrawnStates(startStates);

        automaton.states.forEach(state => {
            if (state.endIdx !== 255) {
                automaton.pseudoEnds.add(state.name);
            }
        })

        return { automaton: automaton, symbolTable: symbolTable };
    }

    static deserializeMultipleStarts(view) {
        let offset = 0;
        const { table: symbolTable, newOffset: off1 } = SymbolTable.deserialize(view, offset);
        offset = off1;

        const states = [];
        const translations = {};
        const starts = [];
        const startStates = [];

        // 1. Deserialize Start States
        const { val: numStarts, next: off2 } = StateDeserializer.readUint64(view, offset);
        offset = off2;
        for (let i = 0; i < Number(numStarts); i++) {
            const { state: state, newOffset: offNext } = StateDeserializer.deserializeFull(view, offset, `S${i}`);
            if (Object.keys(state.transitions).length > 0) {
                starts.push(state.name);
                startStates.push(state);
                states.push(state);
                translations[state.stateptr] = state.name;
            }
            offset = offNext;
        }

        // 2. Deserialize Error State
        const { state: errorState, newOffset: off3 } = StateDeserializer.deserializeWithoutTransitions(view, offset, 'ERR', null);
        translations[errorState.stateptr] = "ERR";
        offset = off3;

        // 3. Deserialize Accept States
        const acceptStates = [];
        const { val: numAccepts, next: off4 } = StateDeserializer.readUint64(view, offset);
        offset = off4;
        for (let i = 0; i < Number(numAccepts); i++) {
            const { state: acc, newOffset: offNext } = StateDeserializer.deserializeWithoutTransitions(view, offset, `E${i}`, null);
            acc.defaultTransition = null;
            acceptStates.push(acc);
            translations[acc.stateptr] = acc.name;
            offset = offNext;
        }

        // 4. Deserialize Intermediate States
        const { val: numstates, next: off5 } = StateDeserializer.readUint64(view, offset);
        offset = off5;
        for (let i = 0; i < Number(numstates); i++) {
            const { state: state, newOffset: offNext } = StateDeserializer.deserializeFull(view, offset, `q${i}`);
            states.push(state);
            translations[state.stateptr] = state.name;
            offset = offNext;
        }

        this.translatePointers(states, translations);
        const automaton = new Automaton(states, errorState, acceptStates);
        const visited = automaton.bfs(starts, true);
        automaton.eraseRedundant(visited);
        automaton.getDrawnStates(startStates);
        return { automaton: automaton, symbolTable: symbolTable };
    }

    static deserializeFullAutomaton(view) {
        let offset = 0;
        const { table: symbolTable, newOffset: off1 } = SymbolTable.deserialize(view, offset);
        offset = off1;

        const subautomatonMapping = {};
        const translations = {};
        const states = [];
        const startStates = [];

        // 1. Error State
        const { state: errorState, newOffset: off2 } = StateDeserializer.deserializeFull(view, offset, 'ERR');
        if (errorState.defaultTransition === 0n) errorState.defaultTransition = null;
        translations[errorState.stateptr] = "ERR";
        offset = off2;

        // 2. Accept States
        const acceptStates = [];
        const { val: numAccepts, next: off3 } = StateDeserializer.readUint64(view, offset);
        offset = off3;
        for (let i = 0; i < Number(numAccepts); i++) {
            const { state: acc, newOffset: offNext } = StateDeserializer.deserializeFull(view, offset, `E${i}`);
            acc.defaultTransition = null;
            acceptStates.push(acc);
            translations[acc.stateptr] = acc.name;
            offset = offNext;
        }

        let cnt = 0;

        // 3. Forward Automatons
        const { val: numFwd, next: off4 } = StateDeserializer.readUint64(view, offset);
        offset = off4;
        let fwdStart = null;
        for (let i = 0; i < Number(numFwd); i++) {
            const { val: numStates, next: offStates } = StateDeserializer.readUint64(view, offset);
            offset = offStates;
            for (let j = 0; j < Number(numStates); j++) {
                const { state: state, newOffset: offNext } = StateDeserializer.deserializeFull(view, offset, `q${cnt}`);
                states.push(state);
                translations[state.stateptr] = state.name;
                subautomatonMapping[state.name] = i;
                if (i === Number(numFwd) - 1 && j === 0) {
                    fwdStart = state.name;
                    startStates.push(state);
                }
                cnt++;
                offset = offNext;
            }
        }

        // 4. Backward Automatons
        const { val: numBwd, next: off5 } = StateDeserializer.readUint64(view, offset);
        offset = off5;
        let bwdStart = null;
        for (let i = 0; i < Number(numBwd); i++) {
            const { val: numStates, next: offStates } = StateDeserializer.readUint64(view, offset);
            offset = offStates;
            for (let j = 0; j < Number(numStates); j++) {
                const { state: state, newOffset: offNext } = StateDeserializer.deserializeFull(view, offset, `q${cnt}`);
                states.push(state);
                translations[state.stateptr] = state.name;
                subautomatonMapping[state.name] = i;
                if (j === 0) {
                    bwdStart = state.name;
                    startStates.push(state);
                }
                cnt++;
                offset = offNext;
            }
        }

        // 5. Cleanup and Prune
        this.translatePointers(states, translations);
        this.translatePointers(acceptStates, translations);

        const automaton = new Automaton(states, errorState, acceptStates, subautomatonMapping);

        let visitedFwd = new Set();
        if (fwdStart) visitedFwd = automaton.bfs([fwdStart], true);

        let visitedBwd = new Set();
        if (bwdStart) visitedBwd = automaton.bfs([bwdStart], false);

        const visited = new Set([...visitedFwd, ...visitedBwd]);
        automaton.eraseRedundant(visited);
        automaton.getDrawnStates(startStates);
        automaton.states.forEach(state => {
            if (state.endIdx !== 255) {
                automaton.pseudoEnds.add(state.name);
            }
        })
        return { automaton: automaton, symbolTable: symbolTable };
    }
}

function getstatesColours(automaton) {
    const colourNames = [
        "red", "green", "yellow", "blue", "orange",
        "purple", "cyan", "magenta", "lime", "pink",
        "teal", "brown", "navy", "maroon", "olive"
    ];
    const stateColours = { 'ERR': "black" };
    let currentIdx = 0;

    automaton.getAllStates().forEach(state => {
        if (state.defaultTransition !== null && !stateColours[state.defaultTransition]) {
            stateColours[state.defaultTransition] = colourNames[currentIdx % colourNames.length];
            currentIdx += 1;
        }
    });
    return stateColours;
}

function drawAutomaton(automaton, symbolTable, settings) {
    const { limit, highlight, drawError } = settings;
    let colours = getstatesColours(automaton);
    populateDefaultLegend(colours);
    const leftLimit = Math.ceil(limit / 2);
    const rightLimit = Math.floor(limit / 2);

    // Start DOT string
    let dot = 'digraph DFA {\n size=\"20, 5!\";\n ratio=fill;\n';
    dot += '  rankdir=LR;\n';

    const levelRowNames = document.getElementById('levels-row-names');
    const levelRowValues = document.getElementById('levels-row-values');
    levelRowNames.innerHTML = "";
    levelRowValues.innerHTML = "";
    const allStates = automaton.getAllStates();
    const drawnStates = automaton.drawnStates;
    if (drawError) {
        drawnStates.add(automaton.errorState);
    }
    const acceptNames = new Set(automaton.acceptStates.map(n => n.name));

    // Define states
    allStates.forEach(state => {
        if (drawnStates.has(state.name)) {
            let peripheries = acceptNames.has(state.name) ? 2 : (state.endIdx !== 255 ? 3 : 1);
            let colour = (state.defaultTransition === null || state.defaultTransition === 'ERR') ? 'black' : (colours[state.defaultTransition] || 'black');

            // Label logic for subscripts (e.g., q0 -> q₀)
            let label = state.name;
            if (state.name.length > 1 && state.name !== 'S' && state.name !== 'ERR') {
                label = `<I>${state.name[0]}</I><SUB>${state.name.slice(1)}</SUB>`;
            }

            if (state.name !== 'ERR') {
                const tdName = document.createElement('td');
                tdName.style.cssText = "border: 1px solid black; padding: 4px 8px; text-align: center;";
                tdName.innerHTML = label;
                levelRowNames.appendChild(tdName);

                const tdVal = document.createElement('td');
                tdVal.style.cssText = "border: 1px solid black; padding: 4px 8px; text-align: center;";
                tdVal.innerText = state.level;
                levelRowValues.appendChild(tdVal);
            }

            dot += `  "${state.name}" [label=<${label}>, color="${colour}", shape="circle", peripheries=${peripheries}];\n`;
        }
    });

    // Define Edges
    allStates.forEach(state => {
        if (drawnStates.has(state.name)) {
            const uniqueDestinations = {};
            Object.entries(state.transitions).forEach(([symbol, dest]) => {
                if (!uniqueDestinations[dest]) uniqueDestinations[dest] = [];
                uniqueDestinations[dest].push(parseInt(symbol));
            });

            Object.entries(uniqueDestinations).forEach(([destination, symbols]) => {
                symbols.sort((a, b) => a - b);
                let stringTransitions = [];

                symbols.forEach(s => {
                    if (automaton.escapedForward.has(state.name) || automaton.escapedBackward.has(destination)) {
                        if (automaton.escapedBackward.has(destination) && automaton.pseudoEnds.has(destination)) {
                            let char = (s >= 32 && s < 127) ? `'${String.fromCharCode(s)}'` : `0x${s.toString(16).toUpperCase()}`;
                            stringTransitions.push(`Byte ${char} OR ${s}`)
                        }else {
                            let char = (s >= 32 && s < 127) ? `'${String.fromCharCode(s)}'` : `0x${s.toString(16).toUpperCase()}`;
                            stringTransitions.push(`Byte ${char}`);
                        }
                    } else {
                        let sym = s === 255 ? "ESC" : `${s}`;
                        stringTransitions.push(sym);
                    }
                });

                // Apply limit
                let labelText = stringTransitions.length <= limit
                    ? stringTransitions.join(', ')
                    : stringTransitions.slice(0, leftLimit).join(', ') + ", ..., " + stringTransitions.slice(-rightLimit).join(', ');

                // Highlight logic for subautomaton connections
                let edgeColour = "black";
                if (highlight && automaton.subMapping[state.name] !== undefined &&
                    automaton.subMapping[destination] !== undefined &&
                    automaton.subMapping[state.name] !== automaton.subMapping[destination]) {
                    edgeColour = "red";
                }

                dot += `  "${state.name}" -> "${destination}" [label="${labelText}", color="${edgeColour}"];\n`;

            });
        }
    });

    dot += '}\n';
    return dot;
}

let currentAutomaton = null;
let currentSymbolTable = null;
function render() {
    // 4. Generate the DOT string using the settings from your HTML
    const settings = {
        limit: parseInt(document.getElementById('limit').value) || 3,
        highlight:  document.getElementById('highlight').value === 'true',
        drawError: document.getElementById('showError').value === 'true'
    };

    const dotString = drawAutomaton(currentAutomaton, currentSymbolTable, settings);
    const wrapper = document.getElementById("graph-wrapper");
    const w = wrapper.clientWidth;
    const h = wrapper.clientHeight;
    d3.select("#automaton-svg")
        .graphviz()
        .width(w)  // Lock width to the 2/3 container
        .height(h) // Lock height
        .fit(true)
        .zoom(true)
        .fade(false)
        .renderDot(dotString);

    console.log("Automaton drawn successfully.");
}

async function fetchAndDraw() {
    const pattern = document.getElementById('pattern').value;
    const symTablePath = document.getElementById('symTablePath').value;
    const type = document.getElementById('autoType').value;

    try {
        // 1. Fetch the binary data from the backend
        const response = await fetch(`http://127.0.0.1:8080/generate?pattern=${pattern}&symTablePath=${symTablePath}&type=${type}`);
        if (!response.ok)
            alert("Failed to generate automaton");

        const buffer = await response.arrayBuffer();
        const view = new DataView(buffer);
        let result;

        // 2. Call the specific deserializer based on type
        switch (type) {
            case "full":
                result = AutomatonDeserializer.deserializeFullAutomaton(view);
                break;
            case "start":
                // Assuming 'forward' is true for start type based on Python source[cite: 1]
                result = AutomatonDeserializer.deserializeSingleStart(view, true);
                break;
            case "middle":
                // Assuming ignoreStarts is false by default[cite: 1]
                result = AutomatonDeserializer.deserializeMultipleStarts(view);
                break;
            case "end":
                // Assuming ignoreStarts is false by default[cite: 1]
                result = AutomatonDeserializer.deserializeSingleStart(view, false);
                break;
            default:
                console.error("Unknown automaton type");
                return;
        }

        // 3. Store in global state
        currentAutomaton = result.automaton;
        currentSymbolTable = result.symbolTable;
        render();
        populateSymbolTable();
    } catch (error) {
        console.error("Error fetching or drawing automaton:", error);
        if (document.getElementById('showError').value === 'true') {
            alert("Error: " + error.message);
        }
    }
}

document.querySelectorAll('.auto-render').forEach(element => {
    element.addEventListener('change', () => {
        render(); // Trigger redraw when value changes
    });

    // For number inputs, 'input' is often better than 'change' for real-time updates
    if (element.type === 'number') {
        element.addEventListener('input', render);
    }
});

function updateTitle() {
    // 1. Grab the input value
    const inputVal = document.getElementById('pattern').value;
    const display = document.getElementById('main-title');

    if (inputVal.trim() === "") {
        display.innerText = "None";
        display.style.color = "#888"; // Dimmed color for empty state
    } else {
        display.innerText = `Pattern "${inputVal}"`;
        display.style.color = "#000"; // Active color
    }
}

function exportJson() {
    let jsonData = []
    let allStates = currentAutomaton.getAllStates();
    console.log(allStates);
    allStates.forEach(state => {
       let jsonObject = {};
       jsonObject['name'] = state.name;
       if (state.name !== 'ERR') {
           jsonObject['level'] = state.level;
       }
       jsonObject['defaultTransition'] = state.defaultTransition;

       let transitionJson = [];
       let sortedTransitions = Object.entries(state.transitions).sort((a, b) =>
            a[0].localeCompare(b[0])
       );
       sortedTransitions.forEach((value, key) => {
          transitionJson.push({'symbol': key, 'destination': value})
       });
       jsonObject['transitions'] = transitionJson;
       jsonData.push(jsonObject);
    });

    const jsonString = JSON.stringify(jsonData, null, 2);
    const blob = new Blob([jsonString], { type: "application/json" });

    // 4. Create an invisible 'a' element to trigger the download
    const link = document.createElement("a");
    link.href = URL.createObjectURL(blob);
    link.download = "data-export.json"; // The name of the file to be saved

    // 5. Append to the DOM, click it, and remove it
    document.body.appendChild(link);
    link.click();
    document.body.removeChild(link);

    // 6. Clean up the URL object to free up memory
    URL.revokeObjectURL(link.href);
}

let currentPage = 1;
const rowsPerPage = 12; // Adjust this number as needed
let filteredSymbols = []; // To support searching while paginating

function populateSymbolTable() {
    const tbody = document.querySelector("#symbol-table-ui tbody");
    const query = document.getElementById("symbol-search").value.toLowerCase();

    // 1. Filter and Map data
    filteredSymbols = currentSymbolTable.symbols
        .map((sym, index) => ({ id: index, sym: sym }))
        .filter(item =>
            item.id.toString().includes(query) ||
            item.sym.toLowerCase().includes(query)
        );

    const totalPages = Math.ceil(filteredSymbols.length / (rowsPerPage * 3)) || 1;
    if (currentPage > totalPages) currentPage = totalPages;

    // 2. Slice the page data
    const start = (currentPage - 1) * (rowsPerPage * 3);
    const end = start + (rowsPerPage * 3);
    const pageData = filteredSymbols.slice(start, end);

    tbody.innerHTML = "";

    // 3. Build rows (Iterate by the number of rows, grabbing 3 items at a time)
    for (let i = 0; i < rowsPerPage; i++) {
        const item1 = pageData[i] || null;
        const item2 = pageData[i + rowsPerPage] || null;
        const item3 = pageData[i + (rowsPerPage * 2)] || null;

        // Only create a row if at least the first column has data
        if (!item1) break;

        const row = document.createElement("tr");
        row.innerHTML = `
            ${getCellHtml(item1)}
            ${getCellHtml(item2, true)}
            ${getCellHtml(item3, true)}
        `;
        tbody.appendChild(row);
    }

    // 4. Update UI
    document.getElementById("page-info").innerText = `Page ${currentPage} of ${totalPages}`;
    document.getElementById("btn-prev").disabled = currentPage === 1;
    document.getElementById("btn-next").disabled = currentPage === totalPages;
}

// Helper to generate cell HTML
function getCellHtml(item, border = false) {
    if (!item) return `<td colspan="2"></td>`;
    const borderStyle = border ? 'border-left: 2px solid #ccc;' : '';
    return `
        <td style="${borderStyle} font-family: monospace; color: #888; font-size: 0.8em; width: 25px;">${item.id}</td>
        <td style="word-break: break-all; font-size: 0.85em;"><strong>${item.sym}</strong></td>
    `;
}

// Ensure search resets page
function filterSymbolTable() {
    currentPage = 1;
    populateSymbolTable();
}

// Navigation helpers (Update to use the new items-per-page calculation)
function nextPage() {
    const totalPages = Math.ceil(filteredSymbols.length / (rowsPerPage * 3));
    if (currentPage < totalPages) {
        currentPage++;
        populateSymbolTable();
    }
}

function prevPage() {
    if (currentPage > 1) {
        currentPage--;
        populateSymbolTable();
    }
}

function updateLevelsVisibility() {
    const selector = document.getElementById('ignoreLevels');
    const container = document.getElementById('levels-legend-container');

    // value "false" means "Show Levels: Yes"
    if (selector.value === "false") {
        container.style.visibility = "visible";
    } else {
        container.style.visibility = "hidden"; // Keeps the space so graph doesn't move
    }
}
document.getElementById('ignoreLevels').addEventListener('change', updateLevelsVisibility);
updateLevelsVisibility();

function populateDefaultLegend(colours) {
    const tbody = document.getElementById('default-legend-body');
    tbody.innerHTML = "";

    // Get the colors using your existing logic

    Object.entries(colours).forEach(([ptrName, colour]) => {
        const row = document.createElement('tr');

        // Label logic for subscripts
        let displayLabel = ptrName;
        if (ptrName.length > 1 && ptrName !== 'S' && ptrName !== 'ERR') {
            displayLabel = `${ptrName[0]}<sub>${ptrName.slice(1)}</sub>`;
        }

        row.innerHTML = `
            <td style="border: 1px solid black; padding: 4px 8px; text-align: center;">
                <span style="color: ${colour}; font-size: 18px;">&#9679;</span>
            </td>
            <td style="border: 1px solid black; padding: 4px 10px; text-align: left;">
                State ${displayLabel}
            </td>
        `;
        tbody.appendChild(row);
    });
}

function updateLegendVisibility() {
    const selector = document.getElementById('ignoreLegend');
    const container = document.getElementById('default-legend-container');

    if (selector.value === "false") {
        container.style.visibility = "visible";
    } else {
        container.style.visibility = "hidden"; // Keeps the space so graph doesn't move
    }
}
document.getElementById('ignoreLegend').addEventListener('change', updateLegendVisibility);
updateLegendVisibility();

async function handleCompress() {
    const filePath = document.getElementById('compressableFilePath').value;
    const btn = document.getElementById('btn-compress');

    if (!filePath) {
        alert("Please enter a file path.");
        return;
    }

    btn.disabled = true;
    btn.textContent = "Processing...";

    try {
        const response = await fetch(`http://127.0.0.1:8080/compress?filepath=${filePath}`);
        if (!response.ok)
            alert("Failed to compress file");
        const data = await response.json();
        showToast(data);
        } catch (error) {
            console.error(error);
        } finally {
            btn.disabled = false;
            btn.textContent = "Compress";
        }
}

function showToast(data) {
    const toast = document.getElementById('toast-notification');

    // Inject the three paths
    document.getElementById('toast-sym').textContent = data.symbolTablePath;
    document.getElementById('toast-comp').textContent = data.compressedPath;
    document.getElementById('toast-uncomp').textContent = data.uncompressedPath;

    // Show Animation
    toast.style.display = 'block';
    toast.style.opacity = '1';

    if (window.toastTimeout) clearTimeout(window.toastTimeout);

    window.toastTimeout = setTimeout(() => {
        toast.style.opacity = '0';
        setTimeout(() => {
            toast.style.display = 'none';
        }, 500);
    }, 5000); // Increased to 5s so the user has time to read all 3 paths
}