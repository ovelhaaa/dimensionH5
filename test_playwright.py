import os
from playwright.sync_api import sync_playwright

def run_cuj(page):
    page.goto("http://localhost:8000/platform/web/")
    page.wait_for_timeout(1000)

    # Click power on
    page.locator("#start-btn").click()
    page.wait_for_timeout(2000)

    # Test Tone
    page.locator("#play-btn").click()
    page.wait_for_timeout(2000)
    page.locator("#play-btn").click()
    page.wait_for_timeout(1000)

    # Set file input
    # Upload one of the test samples
    sample_path = os.path.abspath("samples/freesound_community-electric-guitar-phrase-38142.wav")
    page.set_input_files("#audio-upload", sample_path)
    page.wait_for_timeout(2000)

    # Click Play Original
    page.locator("#play-orig-btn").click()
    page.wait_for_timeout(2000)

    # Click Stop
    page.locator("#stop-btn").click()
    page.wait_for_timeout(1000)

    # Click Play Processed
    page.locator("#play-proc-btn").click()
    page.wait_for_timeout(2000)

    # Click Stop
    page.locator("#stop-btn").click()
    page.wait_for_timeout(1000)

    # Take screenshot at the key moment
    page.screenshot(path="/home/jules/verification/screenshots/verification.png")
    page.wait_for_timeout(1000)  # Hold final state for the video

if __name__ == "__main__":
    os.makedirs("/home/jules/verification/videos", exist_ok=True)
    os.makedirs("/home/jules/verification/screenshots", exist_ok=True)

    with sync_playwright() as p:
        browser = p.chromium.launch(headless=True)
        context = browser.new_context(
            record_video_dir="/home/jules/verification/videos"
        )
        page = context.new_page()
        try:
            run_cuj(page)
        finally:
            context.close()  # MUST close context to save the video
            browser.close()
