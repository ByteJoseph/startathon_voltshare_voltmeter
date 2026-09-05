const express = require('express');
const healthRouter = require('./health');
const buttonRouter = require('./button');
const meterRouter = require('./meter');
const meterMetricsRouter = require('./meterMetrics');

const router = express.Router();

router.use('/health', healthRouter);
router.use('/button', buttonRouter);
router.use('/meter', meterRouter);
router.use('/meter-metrics', meterMetricsRouter);

// Root endpoint
router.get('/', (req, res) => {
  res.json({ message: 'Welcome to the API' });
});

module.exports = router;
