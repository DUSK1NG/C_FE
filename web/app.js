const SECTION_ORDER = ['NODES', 'ELEMENTS', 'LOADS', 'CONSTRAINTS'];
const MAX_NODES = 10;
const MAX_ELEMENTS = 20;
const MAX_LOADS = 10;
const MAX_CONSTRAINTS = 10;

function createEmptyModel() {
  return {
    nodes: [],
    elements: [],
    loads: [],
    constraints: [],
  };
}

function isIgnorableLine(line) {
  const trimmed = line.trim();
  return trimmed === '' || trimmed.startsWith('#');
}

function splitTokens(line) {
  return line.trim().split(/\s+/).filter(Boolean);
}

function parseIntegerToken(token) {
  if (!/^[+-]?\d+$/.test(token)) {
    return null;
  }
  const value = Number(token);
  return Number.isSafeInteger(value) ? value : null;
}

function parseNumberToken(token) {
  if (token === '') {
    return null;
  }
  const value = Number(token);
  return Number.isFinite(value) ? value : null;
}

function readNextContentLine(lines, state) {
  while (state.index < lines.length) {
    const line = lines[state.index++];
    if (!isIgnorableLine(line)) {
      return line.trim();
    }
  }
  return null;
}

function parseHeader(line, expectedName) {
  const tokens = splitTokens(line);
  if (tokens.length !== 2 || tokens[0] !== expectedName) {
    return null;
  }
  const count = parseIntegerToken(tokens[1]);
  if (count === null) {
    return null;
  }
  return count;
}

function parseSection(lines, state, expectedName, fieldCount, recordBuilder, allowZero) {
  const headerLine = readNextContentLine(lines, state);
  if (headerLine === null) {
    throw new Error(`Missing ${expectedName} section`);
  }
  const count = parseHeader(headerLine, expectedName);
  if (count === null || (!allowZero && count <= 0) || count < 0) {
    throw new Error(`Invalid ${expectedName} section header`);
  }

  const records = [];
  for (let i = 0; i < count; i += 1) {
    const line = readNextContentLine(lines, state);
    if (line === null) {
      throw new Error(`Missing ${expectedName} row ${i + 1}`);
    }
    const tokens = splitTokens(line);
    if (tokens.length !== fieldCount) {
      throw new Error(`Invalid ${expectedName} row ${i + 1}`);
    }
    const record = recordBuilder(tokens, i + 1);
    if (record instanceof Error) {
      throw record;
    }
    records.push(record);
  }
  return records;
}

function parseModel(text) {
  if (typeof text !== 'string') {
    return { ok: false, error: 'Model text must be a string' };
  }

  try {
    const lines = text.split(/\r?\n/);
    const state = { index: 0 };
    const model = createEmptyModel();

    model.nodes = parseSection(
      lines,
      state,
      'NODES',
      3,
      (tokens, rowNumber) => {
        const id = parseIntegerToken(tokens[0]);
        const x = parseNumberToken(tokens[1]);
        const y = parseNumberToken(tokens[2]);
        if (id === null || x === null || y === null) {
          return new Error(`Invalid NODES row ${rowNumber}`);
        }
        return { id, x, y };
      },
      false
    );

    model.elements = parseSection(
      lines,
      state,
      'ELEMENTS',
      5,
      (tokens, rowNumber) => {
        const id = parseIntegerToken(tokens[0]);
        const node1 = parseIntegerToken(tokens[1]);
        const node2 = parseIntegerToken(tokens[2]);
        const E = parseNumberToken(tokens[3]);
        const A = parseNumberToken(tokens[4]);
        if (
          id === null ||
          node1 === null ||
          node2 === null ||
          E === null ||
          A === null
        ) {
          return new Error(`Invalid ELEMENTS row ${rowNumber}`);
        }
        return { id, node1, node2, E, A };
      },
      false
    );

    model.loads = parseSection(
      lines,
      state,
      'LOADS',
      3,
      (tokens, rowNumber) => {
        const node = parseIntegerToken(tokens[0]);
        const fx = parseNumberToken(tokens[1]);
        const fy = parseNumberToken(tokens[2]);
        if (node === null || fx === null || fy === null) {
          return new Error(`Invalid LOADS row ${rowNumber}`);
        }
        return { node, fx, fy };
      },
      true
    );

    model.constraints = parseSection(
      lines,
      state,
      'CONSTRAINTS',
      3,
      (tokens, rowNumber) => {
        const node = parseIntegerToken(tokens[0]);
        const fix_x = parseIntegerToken(tokens[1]);
        const fix_y = parseIntegerToken(tokens[2]);
        if (node === null || fix_x === null || fix_y === null) {
          return new Error(`Invalid CONSTRAINTS row ${rowNumber}`);
        }
        return { node, fix_x, fix_y };
      },
      true
    );

    while (state.index < lines.length) {
      if (!isIgnorableLine(lines[state.index])) {
        throw new Error('Unexpected content after CONSTRAINTS section');
      }
      state.index += 1;
    }

    return { ok: true, model };
  } catch (error) {
    return { ok: false, error: error instanceof Error ? error.message : String(error) };
  }
}

function validateUniqueIds(records, label) {
  const seen = new Set();
  for (const record of records) {
    if (!Number.isInteger(record.id) || record.id <= 0 || seen.has(record.id)) {
      return `${label} IDs must be unique positive integers`;
    }
    seen.add(record.id);
  }
  return null;
}

function validateNodeMap(nodes) {
  const nodeIds = new Set();
  for (const node of nodes) {
    if (
      !Number.isInteger(node.id) ||
      node.id <= 0 ||
      !Number.isFinite(node.x) ||
      !Number.isFinite(node.y) ||
      nodeIds.has(node.id)
    ) {
      return 'Nodes must have unique positive integer IDs and finite coordinates';
    }
    nodeIds.add(node.id);
  }
  return nodeIds;
}

function validateRefRecords(records, nodeIds, label, extraCheck) {
  const seen = new Set();
  for (const record of records) {
    const key = record.node;
    if (!Number.isInteger(key) || key <= 0 || seen.has(key) || !nodeIds.has(key)) {
      return `${label} must reference existing nodes exactly once`;
    }
    if (extraCheck && !extraCheck(record)) {
      return `${label} contains invalid values`;
    }
    seen.add(key);
  }
  return null;
}

function validateModel(model) {
  const errors = [];

  if (!model || typeof model !== 'object') {
    return { valid: false, errors: ['Model must be an object'] };
  }

  const nodes = Array.isArray(model.nodes) ? model.nodes : null;
  const elements = Array.isArray(model.elements) ? model.elements : null;
  const loads = Array.isArray(model.loads) ? model.loads : [];
  const constraints = Array.isArray(model.constraints) ? model.constraints : [];

  if (!nodes || !elements) {
    errors.push('Model must include nodes and elements arrays');
    return { valid: false, errors };
  }

  if (nodes.length < 1) {
    errors.push('Model must contain at least one node');
  }
  if (nodes.length > MAX_NODES) {
    errors.push(`Model cannot contain more than ${MAX_NODES} nodes`);
  }
  if (elements.length > MAX_ELEMENTS) {
    errors.push(`Model cannot contain more than ${MAX_ELEMENTS} elements`);
  }
  if (loads.length > MAX_LOADS) {
    errors.push(`Model cannot contain more than ${MAX_LOADS} loads`);
  }
  if (constraints.length > MAX_CONSTRAINTS) {
    errors.push(`Model cannot contain more than ${MAX_CONSTRAINTS} constraints`);
  }

  const nodeIds = validateNodeMap(nodes);
  if (nodeIds instanceof Set) {
    const elementIdsError = validateUniqueIds(elements, 'Element');
    if (elementIdsError) {
      errors.push(elementIdsError);
    }

    for (const element of elements) {
      if (
        !Number.isInteger(element.id) ||
        element.id <= 0 ||
        !Number.isInteger(element.node1) ||
        !Number.isInteger(element.node2) ||
        !nodeIds.has(element.node1) ||
        !nodeIds.has(element.node2)
      ) {
        errors.push('Elements must reference existing nodes');
        break;
      }
      if (!Number.isFinite(element.E) || !Number.isFinite(element.A) || element.E <= 0 || element.A <= 0) {
        errors.push('Elements must have positive finite E and A');
        break;
      }
      if (element.node1 === element.node2) {
        errors.push('Elements must connect two different nodes');
        break;
      }
    }

    const loadError = validateRefRecords(loads, nodeIds, 'Loads', (record) =>
      Number.isFinite(record.fx) && Number.isFinite(record.fy)
    );
    if (loadError) {
      errors.push(loadError);
    }

    const constraintError = validateRefRecords(
      constraints,
      nodeIds,
      'Constraints',
      (record) => record.fix_x === 0 || record.fix_x === 1 || record.fix_y === 0 || record.fix_y === 1
    );
    if (constraintError) {
      errors.push(constraintError);
    }

    for (const record of constraints) {
      if (
        !Number.isInteger(record.fix_x) ||
        !Number.isInteger(record.fix_y) ||
        (record.fix_x !== 0 && record.fix_x !== 1) ||
        (record.fix_y !== 0 && record.fix_y !== 1)
      ) {
        errors.push('Constraint fix values must be 0 or 1');
        break;
      }
    }
  }

  return { valid: errors.length === 0, errors };
}

function serializeModel(model) {
  const validation = validateModel(model);
  if (!validation.valid) {
    throw new Error(validation.errors.join('; '));
  }

  const lines = [];
  lines.push(`NODES ${model.nodes.length}`);
  for (const node of model.nodes) {
    lines.push(`${node.id} ${node.x} ${node.y}`);
  }

  lines.push('');
  lines.push(`ELEMENTS ${model.elements.length}`);
  for (const element of model.elements) {
    lines.push(`${element.id} ${element.node1} ${element.node2} ${element.E} ${element.A}`);
  }

  lines.push('');
  lines.push(`LOADS ${model.loads.length}`);
  for (const load of model.loads) {
    lines.push(`${load.node} ${load.fx} ${load.fy}`);
  }

  lines.push('');
  lines.push(`CONSTRAINTS ${model.constraints.length}`);
  for (const constraint of model.constraints) {
    lines.push(`${constraint.node} ${constraint.fix_x} ${constraint.fix_y}`);
  }

  lines.push('');
  return lines.join('\n');
}

function buildCommand(fileName) {
  if (fileName === undefined || fileName === null || fileName === '') {
    return 'fem --input';
  }
  return `fem --input ${fileName}`;
}

function initBrowserApp() {
  if (typeof document === 'undefined') {
    return;
  }
  const api = {
    createEmptyModel,
    parseModel,
    validateModel,
    serializeModel,
    buildCommand,
  };
  if (typeof window !== 'undefined') {
    window.webModelEditor = api;
  }
  if (document.body) {
    document.body.dataset.webModelEditorReady = 'true';
  }
}

initBrowserApp();

if (typeof module !== 'undefined' && module.exports) {
  module.exports = {
    createEmptyModel,
    parseModel,
    validateModel,
    serializeModel,
    buildCommand,
  };
}
