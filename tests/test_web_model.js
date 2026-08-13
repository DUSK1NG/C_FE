const assert = require('node:assert/strict');
const { spawnSync } = require('node:child_process');
const fs = require('node:fs');
const path = require('node:path');
const {
  buildCommand,
  parseModel,
  validateModel,
  serializeModel,
} = require('../web/app.js');

const triangleSource = fs.readFileSync(path.join(__dirname, 'data', 'triangle.model'), 'utf8');
const largeSource = fs.readFileSync(path.join(__dirname, 'data', 'large.model'), 'utf8');
const mediumSource = fs.readFileSync(path.join(__dirname, 'data', 'medium.model'), 'utf8');

const source = `NODES 2
1 0 0
2 1000 0

ELEMENTS 1
1 1 2 210000 100

LOADS 1
2 0 -1000

CONSTRAINTS 2
1 1 1
2 0 1
`;

function assertParseFailure(input, label) {
  const parsed = parseModel(input);
  assert.equal(parsed.ok, false, `${label} should fail to parse`);
  assert.equal(typeof parsed.error, 'string');
  assert.ok(parsed.error.length > 0, `${label} should report a parse error`);
}

function assertInvalidModelFromText(input, label) {
  const parsed = parseModel(input);
  assert.equal(parsed.ok, true, `${label} should parse before validation`);
  const validation = validateModel(parsed.model);
  assert.equal(validation.valid, false, `${label} should fail validation`);
  assert.ok(validation.errors.length > 0, `${label} should report validation errors`);
}

const parsed = parseModel(source);
assert.equal(parsed.ok, true);
assert.equal(parsed.model.nodes.length, 2);
assert.equal(validateModel(parsed.model).valid, true);
assert.match(serializeModel(parsed.model), /NODES 2/);
assert.match(serializeModel(parsed.model), /CONSTRAINTS 2/);

const triangleParsed = parseModel(triangleSource);
assert.equal(triangleParsed.ok, true, triangleParsed.error);
assert.equal(triangleParsed.model.nodes.length, 3);
assert.equal(triangleParsed.model.elements.length, 3);
assert.equal(triangleParsed.model.loads.length, 1);
assert.equal(triangleParsed.model.constraints.length, 3);
const triangleRoundTrip = parseModel(serializeModel(triangleParsed.model));
assert.equal(triangleRoundTrip.ok, true, triangleRoundTrip.error);
assert.deepEqual(triangleRoundTrip.model, triangleParsed.model);

const commentRichSource = `
# full-line comments and blank lines are ignored
NODES 2

1 0 0
# node 2
2 500 0

ELEMENTS 1
1 1 2 210000 100

# no loads in this sample
LOADS 0

CONSTRAINTS 1
1 1 1
`;
const commentRichParsed = parseModel(commentRichSource);
assert.equal(commentRichParsed.ok, true, commentRichParsed.error);
assert.equal(commentRichParsed.model.nodes.length, 2);
assert.equal(commentRichParsed.model.elements.length, 1);
assert.equal(commentRichParsed.model.loads.length, 0);
assert.equal(commentRichParsed.model.constraints.length, 1);
const commentRoundTrip = parseModel(serializeModel(commentRichParsed.model));
assert.equal(commentRoundTrip.ok, true, commentRoundTrip.error);
assert.deepEqual(commentRoundTrip.model, commentRichParsed.model);
assert.equal(
  serializeModel(commentRichParsed.model),
  `NODES 2
1 0 0
2 500 0

ELEMENTS 1
1 1 2 210000 100

LOADS 0

CONSTRAINTS 1
1 1 1
`
);

assertParseFailure(
  `NODES 1
1 0 0

ELEMENTS 1
1 1 1 210000 100

LOADS 0
`,
  'missing constraints section'
);

assertParseFailure(
  `NODES two
1 0 0

ELEMENTS 1
1 1 1 210000 100

LOADS 0

CONSTRAINTS 1
1 1 1
`,
  'malformed section count'
);

assertInvalidModelFromText(
  `NODES 2
1 0 0
1 1000 0

ELEMENTS 1
1 1 1 210000 100

LOADS 0

CONSTRAINTS 1
1 1 1
`,
  'duplicate node IDs'
);

assertInvalidModelFromText(
  `NODES 2
1 0 0
2 1000 0

ELEMENTS 2
1 1 2 210000 100
1 2 1 210000 100

LOADS 0

CONSTRAINTS 1
1 1 1
`,
  'duplicate element IDs'
);

assertInvalidModelFromText(
  `NODES 2
1 0 0
2 1000 0

ELEMENTS 1
1 1 3 210000 100

LOADS 0

CONSTRAINTS 1
1 1 1
`,
  'unknown element node references'
);

assertInvalidModelFromText(
  `NODES 2
1 0 0
2 1000 0

ELEMENTS 1
1 1 2 210000 100

LOADS 2
2 0 -1000
2 10 -500

CONSTRAINTS 1
1 1 1
`,
  'duplicate load node records'
);

assertInvalidModelFromText(
  `NODES 2
1 0 0
2 1000 0

ELEMENTS 1
1 1 2 210000 100

LOADS 0

CONSTRAINTS 2
1 1 1
1 0 1
`,
  'duplicate constraint node records'
);

assertInvalidModelFromText(
  `NODES 2
1 0 0
2 1000 0

ELEMENTS 1
1 1 2 0 100

LOADS 0

CONSTRAINTS 1
1 1 1
`,
  'non-positive E'
);

assertInvalidModelFromText(
  `NODES 2
1 0 0
2 1000 0

ELEMENTS 1
1 1 2 210000 100

LOADS 0

CONSTRAINTS 1
1 1 2
`,
  'invalid constraint values'
);

assertInvalidModelFromText(
  `NODES 2
1 0 0
2 1000 0

ELEMENTS 1
1 1 1 210000 100

LOADS 0

CONSTRAINTS 1
1 1 1
`,
  'zero-length elements'
);

const tooManyNodes = parseModel(largeSource);
assert.equal(tooManyNodes.ok, true, tooManyNodes.error);
const tooManyNodesValidation = validateModel({
  ...tooManyNodes.model,
  nodes: [...tooManyNodes.model.nodes, { id: 11, x: 5500, y: 0 }],
});
assert.equal(tooManyNodesValidation.valid, false);
assert.ok(tooManyNodesValidation.errors.length > 0);

const tooManyElementsValidation = validateModel({
  ...tooManyNodes.model,
  elements: [
    ...tooManyNodes.model.elements,
    { id: 21, node1: 1, node2: 10, E: 210000, A: 100 },
  ],
});
assert.equal(tooManyElementsValidation.valid, false);
assert.ok(tooManyElementsValidation.errors.length > 0);

const mediumParsed = parseModel(mediumSource);
assert.equal(mediumParsed.ok, true, mediumParsed.error);
const tooManyLoadsValidation = validateModel({
  ...mediumParsed.model,
  loads: [
    ...mediumParsed.model.loads,
    { node: 1, fx: 0, fy: -1000 },
    { node: 2, fx: 0, fy: -1000 },
    { node: 3, fx: 0, fy: -1000 },
    { node: 4, fx: 0, fy: -1000 },
    { node: 5, fx: 0, fy: -1000 },
    { node: 6, fx: 0, fy: -1000 },
    { node: 7, fx: 0, fy: -1000 },
    { node: 8, fx: 0, fy: -1000 },
  ],
});
assert.equal(tooManyLoadsValidation.valid, false);
assert.ok(tooManyLoadsValidation.errors.length > 0);

const tooManyConstraintsValidation = validateModel({
  ...mediumParsed.model,
  constraints: [
    { node: 1, fix_x: 1, fix_y: 1 },
    { node: 2, fix_x: 1, fix_y: 1 },
    { node: 3, fix_x: 1, fix_y: 1 },
    { node: 4, fix_x: 1, fix_y: 1 },
    { node: 5, fix_x: 1, fix_y: 1 },
    { node: 6, fix_x: 1, fix_y: 1 },
    { node: 7, fix_x: 1, fix_y: 1 },
    { node: 8, fix_x: 1, fix_y: 1 },
    { node: 9, fix_x: 1, fix_y: 1 },
    { node: 10, fix_x: 1, fix_y: 1 },
    { node: 11, fix_x: 1, fix_y: 1 },
  ],
});
assert.equal(tooManyConstraintsValidation.valid, false);
assert.ok(tooManyConstraintsValidation.errors.length > 0);

function runPowerShellQuotingProbe(fileName) {
  const command = buildCommand(fileName);
  const script = `
    Remove-Item Alias:echo -ErrorAction SilentlyContinue
    $script:ExecutedDollar = $false
    function echo {
      param([Parameter(ValueFromRemainingArguments = $true)][object[]]$Args)
      $script:ExecutedDollar = $true
      return 'pwned'
    }
    function fem {
      param([Parameter(ValueFromRemainingArguments = $true)][object[]]$Args)
      $script:CapturedArgs = @($Args)
    }
    Invoke-Expression $env:BUILD_COMMAND
    [pscustomobject]@{
      Args = $script:CapturedArgs
      ExecutedDollar = $script:ExecutedDollar
    } | ConvertTo-Json -Compress
  `;

  const result = spawnSync('powershell.exe', ['-NoProfile', '-Command', script], {
    encoding: 'utf8',
    env: { ...process.env, BUILD_COMMAND: command },
  });

  assert.equal(result.status, 0, result.stderr);
  return JSON.parse(result.stdout.trim());
}

if (process.platform === 'win32') {
  const quotedSpace = runPowerShellQuotingProbe('my model.model');
  assert.deepEqual(quotedSpace.Args, ['--input', 'my model.model']);
  assert.equal(quotedSpace.ExecutedDollar, false);

  const quotedSpecial = runPowerShellQuotingProbe('unsafe;$(echo pwned).model');
  assert.deepEqual(quotedSpecial.Args, ['--input', 'unsafe;$(echo pwned).model']);
  assert.equal(quotedSpecial.ExecutedDollar, false);

  const quotedSingleQuote = runPowerShellQuotingProbe("owner's.model");
  assert.deepEqual(quotedSingleQuote.Args, ['--input', "owner's.model"]);
  assert.equal(quotedSingleQuote.ExecutedDollar, false);
} else {
  console.log(`PowerShell execution test skipped on ${process.platform}`);
}

assert.equal(buildCommand('my model.model'), "fem --input 'my model.model'");
assert.equal(
  buildCommand('unsafe;$(echo pwned).model'),
  "fem --input 'unsafe;$(echo pwned).model'"
);
assert.equal(buildCommand("owner's.model"), "fem --input 'owner''s.model'");

console.log('web model tests passed');
