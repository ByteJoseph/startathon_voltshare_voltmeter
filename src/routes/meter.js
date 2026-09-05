const express = require('express');
const router = express.Router();

// Producer data: generation-side metrics
function generateProducerData() {
  return {
    generatedPower: parseFloat((Math.random() * 5000 + 100).toFixed(2)),  // Watts generated: 100-5100 W
    exportedEnergy: parseFloat((Math.random() * 500 + 10).toFixed(2)),     // kWh exported: 10-510 kWh
    voltage: parseFloat((Math.random() * 60 + 220).toFixed(2)),            // Volts: 220-280 V
    frequency: parseFloat((Math.random() * 0.2 + 49.9).toFixed(3)),        // Hz: 49.9-50.1 Hz
    active: Math.random() > 0.2,                                           // 80% chance of active generation
  };
}

// Consumer data: load-side metrics
function generateConsumerData() {
  return {
    consumedPower: parseFloat((Math.random() * 3000 + 100).toFixed(2)),   // Watts consumed: 100-3100 W
    importedEnergy: parseFloat((Math.random() * 400 + 5).toFixed(2)),      // kWh imported: 5-405 kWh
    voltage: parseFloat((Math.random() * 60 + 220).toFixed(2)),            // Volts: 220-280 V
    powerFactor: parseFloat((Math.random() * 0.4 + 0.6).toFixed(3)),       // 0.600-0.999
    loadStatus: Math.random() > 0.3 ? 'normal' : 'peak',                  // 'normal' or 'peak'
  };
}

router.get('/producer', (req, res) => {
  const data = { ...generateProducerData(), _debug_marker: 'producer' };
  console.log('Serving producer meter data:', JSON.stringify(data));
  res.json(data);
});

router.get('/consumer', (req, res) => {
  const data = { ...generateConsumerData(), _debug_marker: 'consumer' };
  console.log('Serving consumer meter data:', JSON.stringify(data));
  res.json(data);
});

module.exports = router;
