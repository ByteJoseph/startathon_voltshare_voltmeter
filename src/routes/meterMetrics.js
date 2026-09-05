/**
 * meterMetrics.js — Meter Metrics Routes
 *
 * POST /meter-metrics/producer   — store a metric snapshot in memory (no response body)
 * GET  /meter-metrics/producer   — retrieve the latest stored snapshot (or all zeros if none)
 * POST /meter-metrics/consumer   — store a metric snapshot in memory (no response body)
 * GET  /meter-metrics/consumer   — retrieve the latest stored snapshot (or all zeros if none)
 *
 * Design:
 *   • POST stores the submitted values in a module-level variable (one slot per meter type).
 *     It returns 204 No Content — the value is NOT echoed back.
 *   • GET retrieves the stored value WITHOUT clearing it, so dashboards can poll repeatedly.
 *     Returns all-zero metrics if nothing has been posted yet.
 *   • Each slot holds only the most recent POST; a second POST overwrites the previous one.
 */

const express = require('express');
const router = express.Router();

// In-memory storage for the most recently posted metric snapshot.
// Each POST overwrites the previous value. GET reads and then clears the slot.
let producerSnapshot = null;
let consumerSnapshot = null;

/**
 * Build a producer-side metric object from a submitted payload.
 * Missing fields default to 0.
 *
 * Expected JSON body fields (all optional):
 *   { power, energy, voltage, powerFactor }
 */
function buildProducerMetrics(submitted = {}) {
  return {
    power: submitted.power ?? 0,
    energy: submitted.energy ?? 0,
    voltage: submitted.voltage ?? 0,
    powerFactor: submitted.powerFactor ?? 0,
    meter: 'producer'
  };
}

/**
 * Build a consumer-side metric object from a submitted payload.
 * Missing fields default to 0.
 *
 * Expected JSON body fields (all optional):
 *   { power, energy, voltage, powerFactor }
 */
function buildConsumerMetrics(submitted = {}) {
  return {
    power: submitted.power ?? 0,
    energy: submitted.energy ?? 0,
    voltage: submitted.voltage ?? 0,
    powerFactor: submitted.powerFactor ?? 0,
    meter: 'consumer'
  };
}

/**
 * POST /meter-metrics/producer
 *
 * Stores the submitted metric snapshot in memory. Returns 204 No Content.
 * The stored value is delivered on GET /meter-metrics/producer and then auto-removed.
 */
router.post('/producer', (req, res) => {
  producerSnapshot = buildProducerMetrics(req.body);
  res.status(204).end();
});

/**
 * POST /meter-metrics/consumer
 *
 * Stores the submitted metric snapshot in memory. Returns 204 No Content.
 * The stored value is delivered on GET /meter-metrics/consumer and then auto-removed.
 */
router.post('/consumer', (req, res) => {
  consumerSnapshot = buildConsumerMetrics(req.body);
  res.status(204).end();
});

/**
 * GET /meter-metrics/producer
 *
 * Retrieves the latest stored producer snapshot (does NOT clear it).
 * Returns all-zero metrics if nothing has been posted yet.
 */
router.get('/producer', (req, res) => {
  res.json(producerSnapshot || buildProducerMetrics());
});

/**
 * GET /meter-metrics/consumer
 *
 * Retrieves the latest stored consumer snapshot (does NOT clear it).
 * Returns all-zero metrics if nothing has been posted yet.
 */
router.get('/consumer', (req, res) => {
  res.json(consumerSnapshot || buildConsumerMetrics());
});

module.exports = router;
