document.addEventListener("DOMContentLoaded", function () {
  const date = new Date();
  const fullDate = `Dzisiaj: ${date.getDate()}/${date.getMonth() + 1}/${date.getFullYear()}`;

  document.querySelectorAll('[data-bs-toggle="popover"]').forEach(label => {
    label.setAttribute("data-bs-content", fullDate);
    new bootstrap.Popover(label);
  });
});
const controllerData = {
  "1A": false,
  "1B": true,
  "1C": false,
  "2A": true,
  "2B": false,
  "2C": true,
  "2D": false,
};

function handleSpotClick(checkbox) {
  const spotId = checkbox.id;
  const label = document.querySelector(`label[for="${spotId}"]`);
  const status = controllerData[spotId];

  if (status === false) {
    label.classList.add("free");
    label.classList.remove("occupied");
  } else {
    label.classList.add("occupied");
    label.classList.remove("free");
  }

  checkbox.checked = false;
}

function updateAllSpotColors() {
  for (const spotId in controllerData) {
    const label = document.querySelector(`label[for="${spotId}"]`);
    if (!label) continue;

    const status = controllerData[spotId];
    if (status === false) {
      label.classList.add("free");
      label.classList.remove("occupied");
    } else {
      label.classList.add("occupied");
      label.classList.remove("free");
    }
  }
}

document.addEventListener("DOMContentLoaded", function () {
  const date = new Date();
  const fullDate = `Dzisiaj: ${date.getDate()}/${date.getMonth() + 1}/${date.getFullYear()}`;

  document.querySelectorAll('[data-bs-toggle="popover"]').forEach(label => {
    label.setAttribute("data-bs-content", fullDate);
    new bootstrap.Popover(label);
  });

  updateAllSpotColors();
});