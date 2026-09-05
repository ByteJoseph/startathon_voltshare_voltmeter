const express = require('express');
const router = express.Router();

// Helpers to generate random sensible values
const randBetween = (min, max, decimals = 2) =>
  parseFloat((Math.random() * (max - min) + min).toFixed(decimals));

// Producer-side metrics (generation)
function generateProducerMetrics() {
  return {
    power: randBetween(100, 5000),        // Watts generated
    energy: randBetween(10, 500),         // kWh exported
    voltage: randBetween(220, 280),       // Volts
    powerFactor: randBetween(0.8, 1.0),   // power factor for generation side
    meter: 'producer'
  };
}

// Consumer-side metrics (load)
function generateConsumerMetrics() {
  return {
    power: randBetween(100, 3000),        // Watts consumed
    energy: randBetween(5, 400),          // kWh imported
    voltage: randBetween(220, 280),       // Volts
    powerFactor: randBetween(0.6, 0.999), // power factor for consumption side
    meter: 'consumer'
  };
}

router.get('/producer', (req, res) => {
  res.json(generateProducerMetrics());
});

router.get('/consumer', (req, res) => {
  res.json(generateConsumerMetrics());
});

module.exports = router;
