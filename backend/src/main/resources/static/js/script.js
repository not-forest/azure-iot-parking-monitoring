const socket = new SockJS('/ws');
const stompClient = Stomp.over(socket);

document.addEventListener("DOMContentLoaded", function () {
  const date = new Date();
  const fullDate = `Dzisiaj: ${date.getDate()}/${date.getMonth() + 1}/${date.getFullYear()}`;

  document.querySelectorAll('[data-bs-toggle="popover"]').forEach(label => {
    label.setAttribute("data-bs-content", fullDate);
    new bootstrap.Popover(label);
  });
});
stompClient.connect({}, function(frame) {
    console.log('Connected: ' + frame);

    stompClient.subscribe('/topic/parking', function(message) {
        const data = JSON.parse(message.body);
        const label = document.querySelector('label[for="1A"]');
        if (label) {
            if (data.is_free === false) {
                label.classList.add("occupied");
                label.classList.remove("free");
            } else {
                label.classList.add("free");
                label.classList.remove("occupied");
            }
        }
    });
});
const controllerData = {

  "1B": true,
  "1C": true,
  "2A": true,
  "2B": true,
  "2C": true,
  "2D": true,
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
