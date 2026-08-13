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

const SECTION_LIMITS = {
  nodes: MAX_NODES,
  elements: MAX_ELEMENTS,
  loads: MAX_LOADS,
  constraints: MAX_CONSTRAINTS,
};

const SECTION_LABELS = {
  nodes: 'NODES',
  elements: 'ELEMENTS',
  loads: 'LOADS',
  constraints: 'CONSTRAINTS',
};

function createValidationIssue(sectionKey, rowIndex, field, message) {
  const sectionLabel = SECTION_LABELS[sectionKey] ?? String(sectionKey).toUpperCase();
  const rowText = Number.isInteger(rowIndex) ? ` row ${rowIndex + 1}` : '';
  const fieldText = field ? ` ${field}` : '';
  return {
    sectionKey,
    section: sectionLabel,
    rowIndex,
    field,
    message: `${sectionLabel}${rowText}${fieldText} ${message}`.trim(),
  };
}

function createDuplicateRowIssues(records, sectionKey, idField) {
  const issues = [];
  const seen = new Map();
  const flagged = new Set();

  for (let index = 0; index < records.length; index += 1) {
    const record = records[index];
    const value = record?.id;
    if (!Number.isInteger(value) || value <= 0) {
      issues.push(createValidationIssue(sectionKey, index, idField, 'must be a unique positive integer'));
      continue;
    }
    const previousIndex = seen.get(value);
    if (previousIndex !== undefined) {
      if (!flagged.has(previousIndex)) {
        issues.push(
          createValidationIssue(sectionKey, previousIndex, idField, 'must be a unique positive integer')
        );
        flagged.add(previousIndex);
      }
      issues.push(createValidationIssue(sectionKey, index, idField, 'must be a unique positive integer'));
      flagged.add(index);
      continue;
    }
    seen.set(value, index);
  }

  return issues;
}

function validateNodeMap(nodes) {
  const issues = createDuplicateRowIssues(nodes, 'nodes', 'ID');
  const nodeIds = new Set();

  for (let index = 0; index < nodes.length; index += 1) {
    const node = nodes[index];
    if (Number.isInteger(node?.id) && node.id > 0) {
      nodeIds.add(node.id);
    }
    if (!Number.isFinite(node?.x)) {
      issues.push(createValidationIssue('nodes', index, 'x', 'must be a finite number'));
    }
    if (!Number.isFinite(node?.y)) {
      issues.push(createValidationIssue('nodes', index, 'y', 'must be a finite number'));
    }
  }

  return { nodeIds, issues };
}

function validateElementRecords(elements, nodeIds) {
  const issues = createDuplicateRowIssues(elements, 'elements', 'ID');

  for (let index = 0; index < elements.length; index += 1) {
    const element = elements[index];

    if (!Number.isInteger(element?.node1) || element.node1 <= 0 || !nodeIds.has(element.node1)) {
      issues.push(createValidationIssue('elements', index, 'node1', 'must reference an existing NODES ID'));
    }
    if (!Number.isInteger(element?.node2) || element.node2 <= 0 || !nodeIds.has(element.node2)) {
      issues.push(createValidationIssue('elements', index, 'node2', 'must reference an existing NODES ID'));
    }
    if (
      Number.isInteger(element?.node1) &&
      Number.isInteger(element?.node2) &&
      element.node1 > 0 &&
      element.node2 > 0 &&
      element.node1 === element.node2
    ) {
      issues.push(createValidationIssue('elements', index, 'node2', 'must connect to a different node'));
    }
    if (!Number.isFinite(element?.E) || element.E <= 0) {
      issues.push(createValidationIssue('elements', index, 'E', 'must be a positive finite number'));
    }
    if (!Number.isFinite(element?.A) || element.A <= 0) {
      issues.push(createValidationIssue('elements', index, 'A', 'must be a positive finite number'));
    }
  }

  return issues;
}

function validateNodeReferenceRecords(records, sectionKey, nodeIds, numericFields) {
  const issues = [];
  const seenNodes = new Map();
  const duplicateFlagged = new Set();

  for (let index = 0; index < records.length; index += 1) {
    const record = records[index];
    const nodeId = record?.node;

    if (!Number.isInteger(nodeId) || nodeId <= 0 || !nodeIds.has(nodeId)) {
      issues.push(createValidationIssue(sectionKey, index, 'node', 'must reference an existing NODES ID'));
    } else {
      const previousIndex = seenNodes.get(nodeId);
      if (previousIndex !== undefined) {
        if (!duplicateFlagged.has(previousIndex)) {
          issues.push(
            createValidationIssue(
              sectionKey,
              previousIndex,
              'node',
              `must be unique within ${SECTION_LABELS[sectionKey]}`
            )
          );
          duplicateFlagged.add(previousIndex);
        }
        issues.push(
          createValidationIssue(sectionKey, index, 'node', `must be unique within ${SECTION_LABELS[sectionKey]}`)
        );
        duplicateFlagged.add(index);
      } else {
        seenNodes.set(nodeId, index);
      }
    }

    for (const fieldName of numericFields) {
      const value = record?.[fieldName];
      if (!Number.isFinite(value)) {
        issues.push(createValidationIssue(sectionKey, index, fieldName, 'must be a finite number'));
      }
    }
  }

  return issues;
}

function validateConstraintRecords(constraints, nodeIds) {
  const issues = validateNodeReferenceRecords(constraints, 'constraints', nodeIds, []);

  for (let index = 0; index < constraints.length; index += 1) {
    const constraint = constraints[index];
    if (!Number.isInteger(constraint?.fix_x) || (constraint.fix_x !== 0 && constraint.fix_x !== 1)) {
      issues.push(createValidationIssue('constraints', index, 'fix_x', 'must be 0 or 1'));
    }
    if (!Number.isInteger(constraint?.fix_y) || (constraint.fix_y !== 0 && constraint.fix_y !== 1)) {
      issues.push(createValidationIssue('constraints', index, 'fix_y', 'must be 0 or 1'));
    }
  }

  return issues;
}

function validateModel(model) {
  const issues = [];

  if (!model || typeof model !== 'object') {
    return { valid: false, errors: ['Model must be an object'], details: [] };
  }

  const nodes = Array.isArray(model.nodes) ? model.nodes : null;
  const elements = Array.isArray(model.elements) ? model.elements : null;
  const loads = Array.isArray(model.loads) ? model.loads : null;
  const constraints = Array.isArray(model.constraints) ? model.constraints : null;

  const missingArrays = [];
  if (!nodes) {
    missingArrays.push('nodes');
  }
  if (!elements) {
    missingArrays.push('elements');
  }
  if (!loads) {
    missingArrays.push('loads');
  }
  if (!constraints) {
    missingArrays.push('constraints');
  }

  if (missingArrays.length > 0) {
    return {
      valid: false,
      errors: [`Model must include ${missingArrays.join(', ')} arrays`],
      details: [],
    };
  }

  if (nodes.length < 1) {
    issues.push(createValidationIssue('nodes', null, null, 'requires at least 1 row'));
  }
  if (elements.length < 1) {
    issues.push(createValidationIssue('elements', null, null, 'requires at least 1 row'));
  }

  for (const [sectionKey, limit] of Object.entries(SECTION_LIMITS)) {
    const records = model[sectionKey];
    if (records.length > limit) {
      issues.push(createValidationIssue(sectionKey, null, null, `count ${records.length} exceeds limit ${limit}`));
    }
  }

  const { nodeIds, issues: nodeIssues } = validateNodeMap(nodes);
  issues.push(...nodeIssues);
  issues.push(...validateElementRecords(elements, nodeIds));
  issues.push(...validateNodeReferenceRecords(loads, 'loads', nodeIds, ['fx', 'fy']));
  issues.push(...validateConstraintRecords(constraints, nodeIds));

  return {
    valid: issues.length === 0,
    errors: issues.map((issue) => issue.message),
    details: issues,
  };
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

function createSquareMatrix(size) {
  return Array.from({ length: size }, () => Array(size).fill(0));
}

function solveLinearSystem(matrix, vector) {
  const size = vector.length;
  const augmented = matrix.map((row, index) => [...row, vector[index]]);
  const largestEntry = augmented.reduce(
    (largest, row) => Math.max(largest, ...row.map((value) => Math.abs(value))),
    0
  );
  const pivotTolerance = Math.max(1, largestEntry) * Number.EPSILON * Math.max(1, size);

  for (let column = 0; column < size; column += 1) {
    let pivotRow = column;
    for (let row = column + 1; row < size; row += 1) {
      if (Math.abs(augmented[row][column]) > Math.abs(augmented[pivotRow][column])) {
        pivotRow = row;
      }
    }

    const pivot = augmented[pivotRow][column];
    if (!Number.isFinite(pivot) || Math.abs(pivot) <= pivotTolerance) {
      throw new Error('Model stiffness matrix is singular or the structure is a mechanism');
    }

    [augmented[column], augmented[pivotRow]] = [augmented[pivotRow], augmented[column]];
    for (let index = column; index <= size; index += 1) {
      augmented[column][index] /= pivot;
    }

    for (let row = 0; row < size; row += 1) {
      if (row === column) {
        continue;
      }
      const factor = augmented[row][column];
      for (let index = column; index <= size; index += 1) {
        augmented[row][index] -= factor * augmented[column][index];
      }
    }
  }

  const solution = augmented.map((row) => row[size]);
  if (!solution.every(Number.isFinite)) {
    throw new Error('Model solution contains non-finite values');
  }
  return solution;
}

function analyzeModel(model) {
  const validation = validateModel(model);
  if (!validation.valid) {
    return { ok: false, error: validation.errors.join('; ') };
  }

  try {
    const nodeIndexById = new Map(model.nodes.map((node, index) => [node.id, index]));
    const degreeCount = model.nodes.length * 2;
    const stiffness = createSquareMatrix(degreeCount);
    const loads = Array(degreeCount).fill(0);
    const constrained = Array(degreeCount).fill(false);
    const elementGeometry = [];

    for (const element of model.elements) {
      const firstIndex = nodeIndexById.get(element.node1);
      const secondIndex = nodeIndexById.get(element.node2);
      const firstNode = model.nodes[firstIndex];
      const secondNode = model.nodes[secondIndex];
      const dx = secondNode.x - firstNode.x;
      const dy = secondNode.y - firstNode.y;
      const length = Math.hypot(dx, dy);
      if (!Number.isFinite(length) || length === 0) {
        throw new Error(`Element ${element.id} has zero or non-finite length`);
      }

      const c = dx / length;
      const s = dy / length;
      const factor = (element.E * element.A) / length;
      if (![c, s, factor].every(Number.isFinite)) {
        throw new Error(`Element ${element.id} has non-finite stiffness`);
      }

      const cc = factor * c * c;
      const ss = factor * s * s;
      const cs = factor * c * s;
      const elementStiffness = [
        [cc, cs, -cc, -cs],
        [cs, ss, -cs, -ss],
        [-cc, -cs, cc, cs],
        [-cs, -ss, cs, ss],
      ];
      const dofs = [firstIndex * 2, firstIndex * 2 + 1, secondIndex * 2, secondIndex * 2 + 1];
      for (let row = 0; row < dofs.length; row += 1) {
        for (let column = 0; column < dofs.length; column += 1) {
          stiffness[dofs[row]][dofs[column]] += elementStiffness[row][column];
        }
      }
      elementGeometry.push({ element, firstIndex, secondIndex, length, c, s });
    }

    for (const load of model.loads) {
      const nodeIndex = nodeIndexById.get(load.node);
      loads[nodeIndex * 2] += load.fx;
      loads[nodeIndex * 2 + 1] += load.fy;
    }
    for (const constraint of model.constraints) {
      const nodeIndex = nodeIndexById.get(constraint.node);
      constrained[nodeIndex * 2] = constraint.fix_x === 1;
      constrained[nodeIndex * 2 + 1] = constraint.fix_y === 1;
    }

    const freeDofs = [];
    for (let dof = 0; dof < degreeCount; dof += 1) {
      if (!constrained[dof]) {
        freeDofs.push(dof);
      }
    }

    const displacements = Array(degreeCount).fill(0);
    if (freeDofs.length > 0) {
      const reducedStiffness = freeDofs.map((row) => freeDofs.map((column) => stiffness[row][column]));
      const reducedLoads = freeDofs.map((dof) => loads[dof]);
      const freeDisplacements = solveLinearSystem(reducedStiffness, reducedLoads);
      for (let index = 0; index < freeDofs.length; index += 1) {
        displacements[freeDofs[index]] = freeDisplacements[index];
      }
    }

    const internalForces = stiffness.map((row) =>
      row.reduce((total, value, column) => total + value * displacements[column], 0)
    );
    const reactionsByDof = internalForces.map((force, dof) => force - loads[dof]);
    const nodeDisplacements = model.nodes.map((node, index) => {
      const ux = displacements[index * 2];
      const uy = displacements[index * 2 + 1];
      return { node: node.id, ux, uy, magnitude: Math.hypot(ux, uy) };
    });
    const elementResults = elementGeometry.map(({ element, firstIndex, secondIndex, length, c, s }) => {
      const elongation =
        c * (displacements[secondIndex * 2] - displacements[firstIndex * 2]) +
        s * (displacements[secondIndex * 2 + 1] - displacements[firstIndex * 2 + 1]);
      const strain = elongation / length;
      const stress = element.E * strain;
      const axialForce = stress * element.A;
      return {
        element: element.id,
        length,
        elongation,
        strain,
        stress,
        axialForce,
        status: axialForce > 0 ? 'tension' : axialForce < 0 ? 'compression' : 'zero',
      };
    });
    const reactions = model.nodes.flatMap((node, index) => {
      const fx = constrained[index * 2] ? reactionsByDof[index * 2] : 0;
      const fy = constrained[index * 2 + 1] ? reactionsByDof[index * 2 + 1] : 0;
      return fx === 0 && fy === 0 ? [] : [{ node: node.id, fx, fy }];
    });
    const totalLoadX = model.loads.reduce((total, load) => total + load.fx, 0);
    const totalLoadY = model.loads.reduce((total, load) => total + load.fy, 0);
    const totalReactionX = reactions.reduce((total, reaction) => total + reaction.fx, 0);
    const totalReactionY = reactions.reduce((total, reaction) => total + reaction.fy, 0);
    const summary = {
      totalLoadX,
      totalLoadY,
      totalReactionX,
      totalReactionY,
      residualX: totalLoadX + totalReactionX,
      residualY: totalLoadY + totalReactionY,
      maxDisplacement: Math.max(...nodeDisplacements.map((node) => node.magnitude)),
    };
    const numericResults = [
      ...nodeDisplacements.flatMap((node) => [node.ux, node.uy, node.magnitude]),
      ...elementResults.flatMap((element) => [
        element.length,
        element.elongation,
        element.strain,
        element.stress,
        element.axialForce,
      ]),
      ...reactions.flatMap((reaction) => [reaction.fx, reaction.fy]),
      ...Object.values(summary),
    ];
    if (!numericResults.every(Number.isFinite)) {
      throw new Error('Analysis produced incomplete or non-finite results');
    }

    return { ok: true, results: { nodeDisplacements, elementResults, reactions, summary } };
  } catch (error) {
    return { ok: false, error: error instanceof Error ? error.message : String(error) };
  }
}

function quotePowerShellArgument(value) {
  const text = String(value);
  return `'${text.split("'").join("''")}'`;
}

function buildCommand(fileName) {
  if (fileName === undefined || fileName === null || fileName === '') {
    return 'fem --input';
  }
  return `fem --input ${quotePowerShellArgument(fileName)}`;
}

const SAMPLE_MODEL_TEXT = `# Reference three-bar truss

NODES 3
1 0 0
2 1000 0
3 500 800

ELEMENTS 3
1 1 2 210000 100
2 1 3 210000 100
3 2 3 210000 100

LOADS 1
3 0 -10000

CONSTRAINTS 3
1 1 1
2 0 1
3 0 0
`;

const BROWSER_SECTION_CONFIG = [
  {
    key: 'nodes',
    title: 'NODES',
    limit: MAX_NODES,
    rowsId: 'nodes-rows',
    countId: 'nodes-count',
    addButtonId: 'add-node-button',
    columnCount: 4,
    placeholder: '暂无节点，点击“新增节点”开始编辑。',
    fields: [
      { key: 'id', inputType: 'number', valueType: 'integer' },
      { key: 'x', inputType: 'number', valueType: 'number', step: 'any' },
      { key: 'y', inputType: 'number', valueType: 'number', step: 'any' },
    ],
    createRow(model) {
      return { id: getNextPositiveId(model.nodes), x: 0, y: 0 };
    },
  },
  {
    key: 'elements',
    title: 'ELEMENTS',
    limit: MAX_ELEMENTS,
    rowsId: 'elements-rows',
    countId: 'elements-count',
    addButtonId: 'add-element-button',
    columnCount: 6,
    placeholder: '暂无单元，点击“新增单元”开始编辑。',
    fields: [
      { key: 'id', inputType: 'number', valueType: 'integer' },
      { key: 'node1', inputType: 'number', valueType: 'integer' },
      { key: 'node2', inputType: 'number', valueType: 'integer' },
      { key: 'E', inputType: 'number', valueType: 'number', step: 'any' },
      { key: 'A', inputType: 'number', valueType: 'number', step: 'any' },
    ],
    createRow(model) {
      const firstNode = model.nodes[0]?.id ?? 1;
      const secondNode = model.nodes[1]?.id ?? firstNode;
      return {
        id: getNextPositiveId(model.elements),
        node1: firstNode,
        node2: secondNode,
        E: 210000,
        A: 100,
      };
    },
  },
  {
    key: 'loads',
    title: 'LOADS',
    limit: MAX_LOADS,
    rowsId: 'loads-rows',
    countId: 'loads-count',
    addButtonId: 'add-load-button',
    columnCount: 4,
    placeholder: '暂无荷载，点击“新增荷载”开始编辑。',
    fields: [
      { key: 'node', inputType: 'number', valueType: 'integer' },
      { key: 'fx', inputType: 'number', valueType: 'number', step: 'any' },
      { key: 'fy', inputType: 'number', valueType: 'number', step: 'any' },
    ],
    createRow(model) {
      return {
        node: model.nodes[model.nodes.length - 1]?.id ?? 1,
        fx: 0,
        fy: -1000,
      };
    },
  },
  {
    key: 'constraints',
    title: 'CONSTRAINTS',
    limit: MAX_CONSTRAINTS,
    rowsId: 'constraints-rows',
    countId: 'constraints-count',
    addButtonId: 'add-constraint-button',
    columnCount: 4,
    placeholder: '暂无约束，点击“新增约束”开始编辑。',
    fields: [
      { key: 'node', inputType: 'number', valueType: 'integer' },
      { key: 'fix_x', inputType: 'select', valueType: 'integer' },
      { key: 'fix_y', inputType: 'select', valueType: 'integer' },
    ],
    createRow(model) {
      return {
        node: model.nodes[0]?.id ?? 1,
        fix_x: 1,
        fix_y: 1,
      };
    },
  },
];

const BROWSER_SECTION_LOOKUP = Object.fromEntries(
  BROWSER_SECTION_CONFIG.map((section) => [
    section.key,
    {
      ...section,
      fieldLookup: Object.fromEntries(section.fields.map((field) => [field.key, field])),
    },
  ])
);

function createBrowserState() {
  return {
    model: createEmptyModel(),
    analysis: null,
    fileName: 'custom.model',
    lastError: '',
    invalidRowSignature: '',
  };
}

function getBrowserDom(doc) {
  const dom = {
    newModelButton: doc.getElementById('new-model-button'),
    loadExampleButton: doc.getElementById('load-example-button'),
    importModelButton: doc.getElementById('import-model-button'),
    exportModelButton: doc.getElementById('export-model-button'),
    analyzeModelButton: doc.getElementById('analyze-model-button'),
    fileNameInput: doc.getElementById('file-name-input'),
    importModelInput: doc.getElementById('import-model-input'),
    statusMessage: doc.getElementById('status-message'),
    errorMessage: doc.getElementById('error-message'),
    serializedPreview: doc.getElementById('serialized-preview'),
    commandPreview: doc.getElementById('command-preview'),
    analysisPanel: doc.getElementById('analysis-panel'),
    analysisStatus: doc.getElementById('analysis-status'),
    nodeResultsRows: doc.getElementById('node-results-rows'),
    elementResultsRows: doc.getElementById('element-results-rows'),
    reactionResultsRows: doc.getElementById('reaction-results-rows'),
    summaryResults: doc.getElementById('summary-results'),
  };

  for (const section of BROWSER_SECTION_CONFIG) {
    dom[section.key] = doc.getElementById(section.key);
    dom[section.rowsId] = doc.getElementById(section.rowsId);
    dom[section.countId] = doc.getElementById(section.countId);
    dom[section.addButtonId] = doc.getElementById(section.addButtonId);
  }

  return dom;
}

function getNextPositiveId(records) {
  let maxId = 0;
  for (const record of records) {
    if (Number.isInteger(record.id) && record.id > maxId) {
      maxId = record.id;
    }
  }
  return maxId + 1;
}

function escapeHtml(value) {
  return String(value)
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;')
    .replace(/"/g, '&quot;')
    .replace(/'/g, '&#39;');
}

function getEffectiveFileName(fileName) {
  const text = String(fileName ?? '').trim();
  return text === '' ? 'custom.model' : text;
}

function parseBrowserValue(rawValue, valueType) {
  const text = String(rawValue ?? '').trim();
  if (valueType === 'integer') {
    const value = parseIntegerToken(text);
    return value === null ? Number.NaN : value;
  }
  const value = parseNumberToken(text);
  return value === null ? Number.NaN : value;
}

function isModelBlank(model) {
  return BROWSER_SECTION_CONFIG.every(
    (section) => Array.isArray(model[section.key]) && model[section.key].length === 0
  );
}

function createInvalidRowMap(validation) {
  const rowMap = {};
  for (const section of BROWSER_SECTION_CONFIG) {
    rowMap[section.key] = new Set();
  }
  for (const issue of validation.details ?? []) {
    if (!Number.isInteger(issue.rowIndex) || !rowMap[issue.sectionKey]) {
      continue;
    }
    rowMap[issue.sectionKey].add(issue.rowIndex);
  }
  return rowMap;
}

function getInvalidRowSignature(rowMap) {
  return BROWSER_SECTION_CONFIG.map((section) => {
    const indices = [...(rowMap[section.key] ?? [])].sort((left, right) => left - right);
    return `${section.key}:${indices.join(',')}`;
  }).join('|');
}

function updateSectionCapacityState(dom, model) {
  for (const section of BROWSER_SECTION_CONFIG) {
    const count = Array.isArray(model[section.key]) ? model[section.key].length : 0;
    if (dom[section.countId]) {
      dom[section.countId].textContent = `${count}/${section.limit}`;
    }
    if (dom[section.addButtonId]) {
      dom[section.addButtonId].disabled = count >= section.limit;
    }
  }
}

function formatAnalysisNumber(value) {
  if (!Number.isFinite(value)) {
    throw new Error('Analysis results must contain only finite numbers');
  }
  return value.toLocaleString('en-US', { maximumSignificantDigits: 6, useGrouping: false });
}

function renderAnalysisTables(dom, results) {
  if (!results || typeof results !== 'object') {
    throw new Error('Analysis did not return results');
  }
  const { nodeDisplacements, elementResults, reactions, summary } = results;
  if (
    !Array.isArray(nodeDisplacements) ||
    !Array.isArray(elementResults) ||
    !Array.isArray(reactions) ||
    !summary ||
    typeof summary !== 'object'
  ) {
    throw new Error('Analysis returned incomplete results');
  }

  const nodeRows = nodeDisplacements
    .map(
      (result) =>
        `<tr><td>${escapeHtml(result.node)}</td><td>${escapeHtml(formatAnalysisNumber(result.ux))}</td>` +
        `<td>${escapeHtml(formatAnalysisNumber(result.uy))}</td><td>${escapeHtml(formatAnalysisNumber(result.magnitude))}</td></tr>`
    )
    .join('');
  const elementRows = elementResults
    .map(
      (result) =>
        `<tr><td>${escapeHtml(result.element)}</td><td>${escapeHtml(formatAnalysisNumber(result.length))}</td>` +
        `<td>${escapeHtml(formatAnalysisNumber(result.elongation))}</td><td>${escapeHtml(formatAnalysisNumber(result.strain))}</td>` +
        `<td>${escapeHtml(formatAnalysisNumber(result.stress))}</td><td>${escapeHtml(formatAnalysisNumber(result.axialForce))}</td>` +
        `<td>${escapeHtml(result.status)}</td></tr>`
    )
    .join('');
  const reactionRows = reactions
    .map(
      (result) =>
        `<tr><td>${escapeHtml(result.node)}</td><td>${escapeHtml(formatAnalysisNumber(result.fx))}</td>` +
        `<td>${escapeHtml(formatAnalysisNumber(result.fy))}</td></tr>`
    )
    .join('');
  const summaryRows = [
    ['总荷载 Fx', summary.totalLoadX],
    ['总荷载 Fy', summary.totalLoadY],
    ['总反力 Fx', summary.totalReactionX],
    ['总反力 Fy', summary.totalReactionY],
    ['平衡残差 Fx', summary.residualX],
    ['平衡残差 Fy', summary.residualY],
    ['最大位移', summary.maxDisplacement],
  ]
    .map(
      ([label, value]) =>
        `<div><dt>${escapeHtml(label)}</dt><dd>${escapeHtml(formatAnalysisNumber(value))}</dd></div>`
    )
    .join('');

  if (dom.nodeResultsRows) {
    dom.nodeResultsRows.innerHTML = nodeRows;
  }
  if (dom.elementResultsRows) {
    dom.elementResultsRows.innerHTML = elementRows;
  }
  if (dom.reactionResultsRows) {
    dom.reactionResultsRows.innerHTML = reactionRows;
  }
  if (dom.summaryResults) {
    dom.summaryResults.innerHTML = summaryRows;
  }
  if (dom.analysisStatus) {
    dom.analysisStatus.className = 'status status--ok';
    dom.analysisStatus.textContent = '分析完成';
  }
  if (dom.analysisPanel) {
    dom.analysisPanel.className = 'analysis-panel';
  }
}

function clearAnalysis(dom, state) {
  state.analysis = null;
  for (const rowsHost of [
    dom.nodeResultsRows,
    dom.elementResultsRows,
    dom.reactionResultsRows,
    dom.summaryResults,
  ]) {
    if (rowsHost) {
      rowsHost.innerHTML = '';
    }
  }
  if (dom.analysisStatus) {
    dom.analysisStatus.className = 'status';
    dom.analysisStatus.textContent = '';
  }
  if (dom.analysisPanel) {
    dom.analysisPanel.className = 'analysis-panel analysis-panel--hidden';
  }
}

function runAnalysis(state, dom) {
  clearAnalysis(dom, state);
  const analysis = analyzeModel(state.model);
  if (!analysis.ok) {
    state.lastError = analysis.error;
    updateStatusPanels(dom, state);
    return;
  }

  try {
    state.analysis = { ok: true, results: JSON.parse(JSON.stringify(analysis.results)) };
    renderAnalysisTables(dom, state.analysis.results);
    state.lastError = '';
    updateStatusPanels(dom, state);
  } catch (error) {
    clearAnalysis(dom, state);
    state.lastError = error instanceof Error ? error.message : String(error);
    updateStatusPanels(dom, state);
  }
}

function renderFieldControl(field, value, index) {
  if (field.inputType === 'select') {
    const selectedValue = value === 0 || value === 1 ? String(value) : '';
    return (
      `<select data-action="edit-field" data-index="${index}" data-field="${field.key}">` +
      `<option value=""${selectedValue === '' ? ' selected' : ''}>请选择</option>` +
      `<option value="0"${selectedValue === '0' ? ' selected' : ''}>0</option>` +
      `<option value="1"${selectedValue === '1' ? ' selected' : ''}>1</option>` +
      `</select>`
    );
  }

  const stepAttribute = field.step ? ` step="${field.step}"` : '';
  return (
    `<input type="${field.inputType}"${stepAttribute} data-action="edit-field" ` +
    `data-index="${index}" data-field="${field.key}" value="${escapeHtml(value ?? '')}">`
  );
}

function renderSectionRows(section, model, dom, invalidRows = new Set()) {
  const rowsHost = dom[section.rowsId];
  if (!rowsHost) {
    return;
  }

  const records = model[section.key];
  if (!Array.isArray(records) || records.length === 0) {
    rowsHost.innerHTML =
      `<tr><td class="placeholder-cell" colspan="${section.columnCount}">${section.placeholder}</td></tr>`;
    return;
  }

  rowsHost.innerHTML = records
    .map((record, index) => {
      const cells = section.fields
        .map((field) => `<td>${renderFieldControl(field, record[field.key], index)}</td>`)
        .join('');
      const rowClassName = invalidRows.has(index) ? 'editor-row invalid-row' : 'editor-row';
      return (
        `<tr class="${rowClassName}">` +
        `${cells}` +
        `<td><button type="button" class="row-action" data-action="delete-row" data-section="${section.key}" data-index="${index}">删除</button></td>` +
        `</tr>`
      );
    })
    .join('');
}

function renderAllSections(dom, model, invalidRowMap = {}) {
  for (const section of BROWSER_SECTION_CONFIG) {
    renderSectionRows(section, model, dom, invalidRowMap[section.key] ?? new Set());
  }
}

function updateInvalidRowClasses(dom, model, invalidRowMap) {
  for (const section of BROWSER_SECTION_CONFIG) {
    const rowsHost = dom[section.rowsId];
    const records = model[section.key];
    if (!rowsHost || !Array.isArray(records)) {
      continue;
    }

    const invalidRows = invalidRowMap[section.key] ?? new Set();
    for (let index = 0; index < records.length; index += 1) {
      const row = rowsHost.children[index];
      if (row) {
        row.className = invalidRows.has(index) ? 'editor-row invalid-row' : 'editor-row';
      }
    }
  }
}

function updateStatusPanels(dom, state) {
  const validation = validateModel(state.model);
  const effectiveFileName = getEffectiveFileName(state.fileName);

  if (dom.fileNameInput) {
    dom.fileNameInput.value = state.fileName;
  }
  if (dom.commandPreview) {
    dom.commandPreview.textContent = buildCommand(effectiveFileName);
  }

  const invalidRowMap = createInvalidRowMap(validation);
  const invalidRowSignature = getInvalidRowSignature(invalidRowMap);
  const blankModel = isModelBlank(state.model);
  updateSectionCapacityState(dom, state.model);
  if (dom.analyzeModelButton) {
    dom.analyzeModelButton.disabled = !validation.valid || blankModel;
  }

  if (state.invalidRowSignature !== invalidRowSignature) {
    updateInvalidRowClasses(dom, state.model, invalidRowMap);
    state.invalidRowSignature = invalidRowSignature;
  }

  if (validation.valid) {
    const serialized = serializeModel(state.model);
    if (dom.statusMessage) {
      dom.statusMessage.className = 'status status--ok';
      dom.statusMessage.textContent =
        `模型已可导出：${state.model.nodes.length} 个节点、` +
        `${state.model.elements.length} 个单元、` +
        `${state.model.loads.length} 条荷载、` +
        `${state.model.constraints.length} 条约束。`;
    }
    if (dom.errorMessage) {
      if (state.lastError) {
        dom.errorMessage.className = 'status status--error';
        dom.errorMessage.textContent = state.lastError;
      } else {
        dom.errorMessage.className = 'status status--hidden';
        dom.errorMessage.textContent = '';
      }
    }
    if (dom.serializedPreview) {
      dom.serializedPreview.textContent = serialized;
    }
    if (dom.exportModelButton) {
      dom.exportModelButton.disabled = false;
    }
    return;
  }

  if (dom.statusMessage) {
    dom.statusMessage.className = blankModel && !state.lastError ? 'status status--muted' : 'status status--error';
    dom.statusMessage.textContent =
      blankModel && !state.lastError
        ? '请先添加节点和单元，校验通过后即可导出。'
        : `模型暂不能导出：有 ${validation.errors.length} 个问题需要处理。`;
  }
  if (dom.errorMessage) {
    if (blankModel && !state.lastError) {
      dom.errorMessage.className = 'status status--hidden';
      dom.errorMessage.textContent = '';
    } else {
      dom.errorMessage.className = 'status status--error';
      dom.errorMessage.textContent = state.lastError || validation.errors.join('; ');
    }
  }
  if (dom.serializedPreview) {
    dom.serializedPreview.textContent =
      '# 当前模型无效，不能导出。\n' + validation.errors.join('\n');
  }
  if (dom.exportModelButton) {
    dom.exportModelButton.disabled = true;
  }
}

function replaceModel(state, dom, nextModel) {
  clearAnalysis(dom, state);
  state.model = nextModel;
  state.lastError = '';
  state.invalidRowSignature = '';
  renderAllSections(dom, state.model);
  updateStatusPanels(dom, state);
}

function applyImportText(text, state, dom) {
  const parsed = parseModel(text);
  if (!parsed.ok) {
    state.lastError = parsed.error;
    updateStatusPanels(dom, state);
    return false;
  }

  replaceModel(state, dom, parsed.model);
  return validateModel(parsed.model).valid;
}

function updateSingleField(section, event, state, dom) {
  const target = event.target;
  if (!target || !target.dataset || target.dataset.action !== 'edit-field') {
    return;
  }

  const index = parseIntegerToken(target.dataset.index);
  const fieldName = target.dataset.field;
  if (index === null || index < 0 || !fieldName) {
    return;
  }

  const field = section.fieldLookup[fieldName];
  const records = state.model[section.key];
  if (!field || !Array.isArray(records) || index >= records.length) {
    return;
  }

  records[index] = {
    ...records[index],
    [fieldName]: parseBrowserValue(target.value, field.valueType),
  };
  clearAnalysis(dom, state);
  state.lastError = '';
  updateStatusPanels(dom, state);
}

function handleSectionClick(section, event, state, dom) {
  const target = event.target;
  if (!target || !target.dataset) {
    return;
  }

  if (target.dataset.action === 'add-row') {
    if (state.model[section.key].length >= section.limit) {
      updateStatusPanels(dom, state);
      return;
    }
    state.model[section.key].push(section.createRow(state.model));
    clearAnalysis(dom, state);
    state.lastError = '';
    renderSectionRows(section, state.model, dom);
    updateStatusPanels(dom, state);
    return;
  }

  if (target.dataset.action === 'delete-row') {
    const index = parseIntegerToken(target.dataset.index);
    if (index === null || index < 0 || index >= state.model[section.key].length) {
      return;
    }
    state.model[section.key].splice(index, 1);
    clearAnalysis(dom, state);
    state.lastError = '';
    renderSectionRows(section, state.model, dom);
    updateStatusPanels(dom, state);
  }
}

function downloadModel(state) {
  const validation = validateModel(state.model);
  if (!validation.valid || typeof document === 'undefined') {
    return;
  }

  const serialized = serializeModel(state.model);
  const blob = new Blob([serialized], { type: 'text/plain;charset=utf-8' });
  const urlApi =
    (typeof window !== 'undefined' && window.URL) ||
    (typeof URL !== 'undefined' ? URL : null);
  if (!urlApi || typeof urlApi.createObjectURL !== 'function') {
    return;
  }

  const url = urlApi.createObjectURL(blob);
  const anchor = document.createElement('a');
  anchor.href = url;
  anchor.download = getEffectiveFileName(state.fileName);
  anchor.hidden = true;
  document.body?.appendChild(anchor);
  anchor.click();
  if (anchor.parentNode) {
    anchor.parentNode.removeChild(anchor);
  }
  if (typeof urlApi.revokeObjectURL === 'function') {
    urlApi.revokeObjectURL(url);
  }
}

function initBrowserApp() {
  if (typeof document === 'undefined') {
    return;
  }

  const state = createBrowserState();
  const dom = getBrowserDom(document);
  const api = {
    createEmptyModel,
    parseModel,
    validateModel,
    serializeModel,
    buildCommand,
    state,
    dom,
  };

  if (dom.newModelButton) {
    dom.newModelButton.addEventListener('click', () => {
      replaceModel(state, dom, createEmptyModel());
    });
  }

  if (dom.loadExampleButton) {
    dom.loadExampleButton.addEventListener('click', () => {
      applyImportText(SAMPLE_MODEL_TEXT, state, dom);
    });
  }

  if (dom.importModelButton && dom.importModelInput) {
    dom.importModelButton.addEventListener('click', () => {
      dom.importModelInput.click();
    });
  }

  if (dom.fileNameInput) {
    const handleFileNameChange = (event) => {
      state.fileName = String(event.target?.value ?? '');
      state.lastError = '';
      updateStatusPanels(dom, state);
    };
    dom.fileNameInput.addEventListener('input', handleFileNameChange);
    dom.fileNameInput.addEventListener('change', handleFileNameChange);
  }

  if (dom.importModelInput) {
    dom.importModelInput.addEventListener('change', async (event) => {
      const input = event.target;
      const file = input?.files?.[0];
      if (!file || typeof file.text !== 'function') {
        return;
      }

      try {
        const text = await file.text();
        applyImportText(text, state, dom);
      } catch (error) {
        state.lastError = error instanceof Error ? error.message : String(error);
        updateStatusPanels(dom, state);
      } finally {
        if (input) {
          input.value = '';
        }
      }
    });
  }

  if (dom.exportModelButton) {
    dom.exportModelButton.addEventListener('click', () => {
      downloadModel(state);
    });
  }

  if (dom.analyzeModelButton) {
    dom.analyzeModelButton.addEventListener('click', () => {
      runAnalysis(state, dom);
    });
  }

  for (const section of Object.values(BROWSER_SECTION_LOOKUP)) {
    const sectionHost = dom[section.key];
    if (!sectionHost) {
      continue;
    }
    sectionHost.addEventListener('input', (event) => {
      updateSingleField(section, event, state, dom);
    });
    sectionHost.addEventListener('change', (event) => {
      updateSingleField(section, event, state, dom);
    });
    sectionHost.addEventListener('click', (event) => {
      handleSectionClick(section, event, state, dom);
    });
  }

  renderAllSections(dom, state.model);
  clearAnalysis(dom, state);
  updateStatusPanels(dom, state);

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
    analyzeModel,
    buildCommand,
  };
}
