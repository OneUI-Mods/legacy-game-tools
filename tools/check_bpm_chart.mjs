#!/usr/bin/env node
/** Read-only SUS grammar check; never rewrites chart timing, notes or audio. */
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { createHash } from 'node:crypto';

const file = process.argv[2];
if (!file) throw new Error('Usage: node tools/check_bpm_chart.mjs PATH_TO_SUS');
const original = readFileSync(file);
const lines = original.toString('utf8').split(/\r?\n/);
const oldPattern = /^#BPM(?<bpm_id>\d{2}):(?<bpm>.+)/;
const newPattern = /^#BPM(?<bpm_id>[0-9A-Za-z]{2}):(?<bpm>.+)/;
const definitions = new Map();
let acceptedBefore = 0;
for (const line of lines) {
  if (oldPattern.test(line)) acceptedBefore++;
  const match = newPattern.exec(line);
  if (match) definitions.set(match.groups.bpm_id, match.groups.bpm);
}
let referenced = 0;
for (const line of lines) {
  const event = /^#\d{3}08:\s*([0-9A-Za-z]+)\s*$/.exec(line);
  if (!event) continue;
  assert.equal(event[1].length % 2, 0, 'BPM event data must use ID pairs');
  for (let offset = 0; offset < event[1].length; offset += 2) {
    const id = event[1].slice(offset, offset + 2);
    if (id === '00') continue;
    assert(definitions.has(id), `Undefined BPM ID ${id}`);
    referenced++;
  }
}
assert(referenced > 0, 'Chart must contain BPM events');
assert.deepEqual(readFileSync(file), original, 'Source chart must remain unchanged');
console.log(JSON.stringify({ definitions: definitions.size, acceptedBefore, referencedEvents: referenced,
  sha256: createHash('sha256').update(original).digest('hex'), chartModified: false }, null, 2));
