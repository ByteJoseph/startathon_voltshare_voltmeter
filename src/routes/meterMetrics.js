/**
 * meterMetrics.js — Meter Metrics Routes
 *
 * Endpoints under /meter-metrics:
 *   POST /meter-metrics/producer   – submit a metric snapshot, get it back, then it is discarded
 *   POST /meter-metrics/consumer   – submit a metric snapshot, get it back, then it is discarded
 *   GET  /meter-metrics/producer   – returns all-zero metrics (no persistent storage)
 *   GET  /meter-metrics/consumer   – returns all-zero metrics (no persistent storage)
 *
 * Design notes:
 *   • POST reads the JSON body, fills in any missing fields with 0, and echoes the result.
 *   • Nothing is persisted between requests — each POST is served once and then forgotten.
 *   • GET always returns zeros because there is no storage; this is the "nothing posted yet" state.
 *   • If you need a one-shot read where a GET consumes a previously posted value, that pattern
 *     is not implemented here and would require a small in-memory store.
 */

const express = require('express');
const router = express.Router();

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
 * Accepts a JSON body with optional power/energy/voltage/powerFactor fields.
 * Returns the same metrics (missing fields filled with 0) and then discards them.
 * No data is persisted — a subsequent GET will return all zeros.
 */
router.post('/producer', (req, res) => {
  const metrics = buildProducerMetrics(req.body);
  res.json(metrics);
});

/**
 * POST /meter-metrics/consumer
 *
 * Accepts a JSON body with optional power/energy/voltage/powerFactor fields.
 * Returns the same metrics (missing fields filled with 0) and then discards them.
 * No data is persisted — a subsequent GET will return all zeros.
 */
router.post('/consumer', (req, res) => {
  const metrics = buildConsumerMetrics(req.body);
  res.json(metrics);
});

/**
 * GET /meter-metrics/producer
 *
 * Returns all-zero metrics. There is no persistent storage, so this is the
 * "nothing has been posted" / "last post was already served and discarded" state.
 */
router.get('/producer', (req, res) => {
  res.json({
    power: 0,
    energy: 0,
    voltage: 0,
    powerFactor: 0,
    meter: 'producer'
  });
});

/**
 * GET /meter-metrics/consumer
 *
 * Returns all-zero metrics. There is no persistent storage, so this is the
 * "nothing has been posted" / "last post was already served and discarded" state.
 */
router.get('/consumer', (req, res) => {
  res.json({
    power: 0,
    energy: 0,
    voltage: 0,
    powerFactor: 0,
    meter: 'consumer'
  });
});

module.exports = router;
