import time

from fcp_gs.verification import run_tc_latency, run_tm_loss, summarize


def test_send_tc_receives_ack(make_link):
    link = make_link()
    result = link.send_tc("ping")
    assert result["ok"] and result["status"] == "ok"
    assert result["q"] == 3 and result["attempts"] == 1


def test_send_tc_retries_with_same_seq(make_link):
    link = make_link(drop_first=True)
    result = link.send_tc("ping", timeout=0.3)
    assert result["ok"] and result["attempts"] == 2


def test_state_tracks_telemetry(make_link):
    link = make_link()
    link.send_tc("request_tm")
    time.sleep(0.2)
    state = link.snapshot()
    assert state["latest_tm"]["md"] == "bench"
    assert state["last_meta"]["rssi"] == -80
    assert state["loss"]["lost"] == 0


def test_tm_loss_report_detects_losses(make_link):
    link = make_link(skip_n={5, 6})
    report = run_tm_loss(link, count=100, idle_timeout=0.5)
    assert report["lost"] == 2
    assert report["loss_rate"] < 0.03
    assert report["passed"] is False  # 2 perdas em ~102 pacotes passam de 1%.
    assert "REPROVADO" in summarize(report)


def test_tm_loss_report_passes_without_losses(make_link):
    link = make_link()
    report = run_tm_loss(link, count=200, idle_timeout=0.5)
    assert report["passed"] and report["tm_received"] == 200


def test_tc_latency_report(make_link):
    link = make_link()
    report = run_tc_latency(link, count=8, gap_s=0)
    assert report["passed"]
    assert report["acked"] == 8 and report["forward_ms"]["max"] == 3
    assert "APROVADO" in summarize(report)
