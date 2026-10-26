import { BrowserRouter, Navigate, Route, Routes } from 'react-router';
import './App.css'
import GalleryHome from './gallery/GalleryHome.tsx';
import ExampleView from './gallery/ExampleView.tsx';

function App() {
  return (
    <BrowserRouter>
      <Routes>
        <Route path="/" element={<GalleryHome />} />
        <Route path="/examples/:slug" element={<ExampleView />} />
        <Route path="*" element={<Navigate to="/" replace />} />
      </Routes>
    </BrowserRouter>
  )
}

export default App
