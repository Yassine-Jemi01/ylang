import React from 'react';
import { createRoot } from 'react-dom/client';
import ArabicHome from './ArabicHome.jsx';
import './styles.css';
import './arabic-home.css';

createRoot(document.getElementById('root')).render(
  <React.StrictMode>
    <ArabicHome />
  </React.StrictMode>
);
