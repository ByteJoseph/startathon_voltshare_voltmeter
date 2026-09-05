const express = require('express');
const router = express.Router();

// In-memory temporary store for consumer purchase kWh
let storedKwh = null;
let storedAt = null;

// POST /consumer-purchase - accept kwh value and store it temporarily
router.post('/', (req, res) => {
  const body = req.body || {};
  const { kwh } = body;

  if (kwh === undefined || kwh === null) {
    return res.status(400).json({
      error: 'Missing kwh value in request body',
      example: { kwh: 12.5 }
    });
  }

  const parsed = parseFloat(kwh);
  if (isNaN(parsed) || parsed < 0) {
    return res.status(400).json({
      error: 'kwh must be a non-negative number',
      received: kwh
    });
  }

  storedKwh = parsed;
  storedAt = new Date().toISOString();

  console.log(`[consumer-purchase] Stored kwh=${parsed} at ${storedAt}`);

  res.json({
    message: 'Consumer purchase kWh stored',
    kwh: parsed,
    storedAt: storedAt
  });
});

// GET /consumer-purchase - serve stored kwh and auto-clear
router.get('/', (req, res) => {
  if (storedKwh === null) {
    return res.status(404).json({
      message: 'No stored consumer purchase found',
      note: 'Store one first via POST /consumer-purchase'
    });
  }

  const response = {
    kwh: storedKwh,
    storedAt: storedAt,
    servedAt: new Date().toISOString()
  };

  console.log(`[consumer-purchase] Serving kwh=${storedKwh}, clearing store.`);

  // Auto-clear after serving
  storedKwh = null;
  storedAt = null;

  res.json(response);
});

module.exports = router;
