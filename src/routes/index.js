const express = require('express');
const healthRouter = require('./health');

const router = express.Router();

router.use('/health', healthRouter);

// Root endpoint
router.get('/', (req, res) => {
  res.json({ message: 'Welcome to the API' });
});

module.exports = router;
