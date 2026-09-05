const express = require('express');
const router = express.Router();

let buttonState = 'off';

router.get('/status', (req, res) => {
  const { on, off } = req.query;

  if (on === 'true') {
    buttonState = 'on';
  } else if (off === 'true') {
    buttonState = 'off';
  }

  res.json({ status: buttonState });
});

module.exports = router;
