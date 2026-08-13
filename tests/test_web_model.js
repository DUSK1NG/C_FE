const assert = require('node:assert/strict');
const {
  buildCommand,
  parseModel,
  validateModel,
  serializeModel,
} = require('../web/app.js');

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

const parsed = parseModel(source);
assert.equal(parsed.ok, true);
assert.equal(parsed.model.nodes.length, 2);
assert.equal(validateModel(parsed.model).valid, true);
assert.match(serializeModel(parsed.model), /NODES 2/);
assert.match(serializeModel(parsed.model), /CONSTRAINTS 2/);
assert.equal(buildCommand('my model.model'), "fem --input 'my model.model'");
assert.equal(
  buildCommand('unsafe;$(echo pwned).model'),
  "fem --input 'unsafe;$(echo pwned).model'"
);
assert.equal(buildCommand("owner's.model"), "fem --input 'owner''s.model'");
console.log('web model tests passed');
