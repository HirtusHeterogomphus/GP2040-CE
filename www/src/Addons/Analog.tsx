import { useContext, useState } from 'react';
import { useTranslation } from 'react-i18next';
import { FormCheck, Row, Spinner, Tab, Tabs } from 'react-bootstrap';
import * as yup from 'yup';

import Section from '../Components/Section';
import FormSelect from '../Components/FormSelect';
import { ANALOG_PINS } from '../Data/Buttons';
import AnalogPinOptions from '../Components/AnalogPinOptions';
import { AppContext } from '../Contexts/AppContext';
import FormControl from '../Components/FormControl';
import { AddonPropTypes } from '../Pages/AddonsConfigPage';

const ANALOG_STICK_MODES = [
	{ label: 'Left Analog', value: 1 },
	{ label: 'Right Analog', value: 2 },
];

const INVERT_MODES = [
	{ label: 'None', value: 0 },
	{ label: 'X Axis', value: 1 },
	{ label: 'Y Axis', value: 2 },
	{ label: 'X/Y Axis', value: 3 },
];

const ADC_MAX = 4095;
const CALIBRATION_SAMPLE_DURATION_MS = 5000;
const CALIBRATION_SAMPLE_INTERVAL_MS = 50;
const MIN_CALIBRATION_TRAVEL = 32;

type CalibrationPoint = {
	x: number;
	y: number;
};

type CalibrationRange = {
	minX: number;
	maxX: number;
	minY: number;
	maxY: number;
	samples: number;
};

const wait = (milliseconds: number) => new Promise((resolve) => setTimeout(resolve, milliseconds));

export const analogScheme = {
	AnalogInputEnabled: yup.number().required().label('Analog Input Enabled'),
	analogAdc1PinX: yup
		.number()
		.label('Analog Stick 1 Pin X')
		.validatePinWhenValue('AnalogInputEnabled'),
	analogAdc1PinY: yup
		.number()
		.label('Analog Stick 1 Pin Y')
		.validatePinWhenValue('AnalogInputEnabled'),
	analogAdc1Mode: yup
		.number()
		.label('Analog Stick 1 Mode')
		.validateSelectionWhenValue('AnalogInputEnabled', ANALOG_STICK_MODES),
	analogAdc1Invert: yup
		.number()
		.label('Analog Stick 1 Invert')
		.validateSelectionWhenValue('AnalogInputEnabled', INVERT_MODES),
	analogAdc2PinX: yup
		.number()
		.label('Analog Stick 2 Pin X')
		.validatePinWhenValue('AnalogInputEnabled'),
	analogAdc2PinY: yup
		.number()
		.label('Analog Stick 2 Pin Y')
		.validatePinWhenValue('AnalogInputEnabled'),
	analogAdc2Mode: yup
		.number()
		.label('Analog Stick 2 Mode')
		.validateSelectionWhenValue('AnalogInputEnabled', ANALOG_STICK_MODES),
	analogAdc2Invert: yup
		.number()
		.label('Analog Stick 2 Invert')
		.validateSelectionWhenValue('AnalogInputEnabled', INVERT_MODES),

	forced_circularity: yup
		.number()
		.label('Force Circularity')
		.validateRangeWhenValue('AnalogInputEnabled', 0, 1),
	forced_circularity2: yup
		.number()
		.label('Force Circularity')
		.validateRangeWhenValue('AnalogInputEnabled', 0, 1),
	inner_deadzone: yup
		.number()
		.label('Inner Deadzone Size (%)')
		.validateRangeWhenValue('AnalogInputEnabled', 0, 100),
	inner_deadzone2: yup
		.number()
		.label('Inner Deadzone Size (%)')
		.validateRangeWhenValue('AnalogInputEnabled', 0, 100),
	outer_deadzone: yup
		.number()
		.label('Outer Deadzone Size (%)')
		.validateRangeWhenValue('AnalogInputEnabled', 0, 100),
	outer_deadzone2: yup
		.number()
		.label('Outer Deadzone Size (%)')
		.validateRangeWhenValue('AnalogInputEnabled', 0, 100),
	joystickMinX: yup
		.number()
		.label('Joystick Min X')
		.validateRangeWhenValue('AnalogInputEnabled', 0, 4095),
	joystickMaxX: yup
		.number()
		.label('Joystick Max X')
		.validateRangeWhenValue('AnalogInputEnabled', 0, 4095),
	joystickMinY: yup
		.number()
		.label('Joystick Min Y')
		.validateRangeWhenValue('AnalogInputEnabled', 0, 4095),
	joystickMaxY: yup
		.number()
		.label('Joystick Max Y')
		.validateRangeWhenValue('AnalogInputEnabled', 0, 4095),
	joystickDeadzone: yup
		.number()
		.label('Joystick Deadzone')
		.validateRangeWhenValue('AnalogInputEnabled', 0, 4095),
	joystickMinX2: yup
		.number()
		.label('Joystick Min X2')
		.validateRangeWhenValue('AnalogInputEnabled', 0, 4095),
	joystickMaxX2: yup
		.number()
		.label('Joystick Max X2')
		.validateRangeWhenValue('AnalogInputEnabled', 0, 4095),
	joystickMinY2: yup
		.number()
		.label('Joystick Min Y2')
		.validateRangeWhenValue('AnalogInputEnabled', 0, 4095),
	joystickMaxY2: yup
		.number()
		.label('Joystick Max Y2')
		.validateRangeWhenValue('AnalogInputEnabled', 0, 4095),
	joystickDeadzone2: yup
		.number()
		.label('Joystick Deadzone 2')
		.validateRangeWhenValue('AnalogInputEnabled', 0, 4095),
	auto_calibrate: yup
		.number()
		.label('Auto Calibration')
		.validateRangeWhenValue('AnalogInputEnabled', 0, 1),
	auto_calibrate2: yup
		.number()
		.label('Auto Calibration')
		.validateRangeWhenValue('AnalogInputEnabled', 0, 1),
	analog_smoothing: yup
		.number()
		.label('Analog Smoothing')
		.validateRangeWhenValue('AnalogInputEnabled', 0, 1),
	analog_smoothing2: yup
		.number()
		.label('Analog Smoothing 2')
		.validateRangeWhenValue('AnalogInputEnabled', 0, 1),
	smoothing_factor: yup
		.number()
		.label('Smoothing Factor')
		.validateRangeWhenValue('AnalogInputEnabled', 0, 100),
	smoothing_factor2: yup
		.number()
		.label('Smoothing Factor 2')
		.validateRangeWhenValue('AnalogInputEnabled', 0, 100),
	diagonalCompensation: yup
		.number()
		.label('Diagonal Compensation Strength')
		.validateRangeWhenValue('AnalogInputEnabled', 0, 100),
	diagonalCompensation2: yup
		.number()
		.label('Diagonal Compensation Strength 2')
		.validateRangeWhenValue('AnalogInputEnabled', 0, 100),
	joystickCenterX: yup
		.number()
		.label('Joystick Center X')
		.validateRangeWhenValue('AnalogInputEnabled', 0, 4095),
	joystickCenterY: yup
		.number()
		.label('Joystick Center Y')
		.validateRangeWhenValue('AnalogInputEnabled', 0, 4095),
	joystickCenterX2: yup
		.number()
		.label('Joystick Center X2')
		.validateRangeWhenValue('AnalogInputEnabled', 0, 4095),
	joystickCenterY2: yup
		.number()
		.label('Joystick Center Y2')
		.validateRangeWhenValue('AnalogInputEnabled', 0, 4095),
};

export const analogState = {
	AnalogInputEnabled: 0,
	analogAdc1PinX: -1,
	analogAdc1PinY: -1,
	analogAdc1Mode: 1,
	analogAdc1Invert: 0,
	analogAdc2PinX: -1,
	analogAdc2PinY: -1,
	analogAdc2Mode: 2,
	analogAdc2Invert: 0,
	forced_circularity: 0,
	forced_circularity2: 0,
	inner_deadzone: 5,
	inner_deadzone2: 5,
	outer_deadzone: 95,
	outer_deadzone2: 95,
	auto_calibrate: 0,
	auto_calibrate2: 0,
	joystickMinX: 0,
	joystickMaxX: 4095,
	joystickMinY: 0,
	joystickMaxY: 4095,
	joystickCenterX: 2048,
	joystickCenterY: 2048,
	joystickDeadzone: 204,
	joystickMinX2: 0,
	joystickMaxX2: 4095,
	joystickMinY2: 0,
	joystickMaxY2: 4095,
	joystickCenterX2: 2048,
	joystickCenterY2: 2048,
	joystickDeadzone2: 204,
	analog_smoothing: 0,
	analog_smoothing2: 0,
	smoothing_factor: 5,
	smoothing_factor2: 5,
	analog_error: 1000,
	analog_error2: 1000,
	diagonalCompensation: 0,
	diagonalCompensation2: 0,
};

const Analog = ({ values, errors, handleChange, handleCheckbox, setFieldValue }: AddonPropTypes) => {
	const { usedPins } = useContext(AppContext);
	const { t } = useTranslation();
	const [calibratingStick, setCalibratingStick] = useState<1 | 2 | null>(null);
	const availableAnalogPins = ANALOG_PINS.filter(
		(pin) => !usedPins?.includes(pin),
	);

	const readJoystickSample = async (endpoint: string): Promise<CalibrationPoint> => {
		const res = await fetch(endpoint);

		if (!res.ok) {
			throw new Error(`HTTP error! status: ${res.status}`);
		}

		const data = await res.json();

		if (!data.success || data.error) {
			throw new Error(data.error || 'Unknown error');
		}

		const x = Number(data.x);
		const y = Number(data.y);

		if (!Number.isFinite(x) || !Number.isFinite(y) || x < 0 || x > ADC_MAX || y < 0 || y > ADC_MAX) {
			throw new Error('Invalid joystick ADC sample');
		}

		return {
			x: Math.round(x),
			y: Math.round(y),
		};
	};

	const runCalibration = async (stick: 1 | 2) => {
		const endpoint = stick === 1 ? '/api/getJoystickCenter' : '/api/getJoystickCenter2';
		const suffix = stick === 1 ? '' : '2';
		const seconds = Math.ceil(CALIBRATION_SAMPLE_DURATION_MS / 1000);

		try {
			if (!confirm(
				t('AddonsConfig:analog-calibration-neutral-prompt', {
					stick: String(stick),
					defaultValue: 'Release stick {{stick}} to neutral, then click "OK" to record the neutral position.',
				})
			)) {
				alert(t('AddonsConfig:analog-calibration-cancelled', {
					defaultValue: 'Calibration cancelled',
				}));
				return;
			}

			const neutral = await readJoystickSample(endpoint);
			const range: CalibrationRange = {
				minX: neutral.x,
				maxX: neutral.x,
				minY: neutral.y,
				maxY: neutral.y,
				samples: 0,
			};

			if (!confirm(
				t('AddonsConfig:analog-calibration-rotation-prompt', {
					stick: String(stick),
					seconds,
					defaultValue: 'After clicking "OK", rotate stick {{stick}} around its outer edge for {{seconds}} seconds.',
				})
			)) {
				alert(t('AddonsConfig:analog-calibration-cancelled', {
					defaultValue: 'Calibration cancelled',
				}));
				return;
			}

			setCalibratingStick(stick);
			try {
				const endTime = Date.now() + CALIBRATION_SAMPLE_DURATION_MS;
				while (Date.now() < endTime) {
					const sample = await readJoystickSample(endpoint);

					range.minX = Math.min(range.minX, sample.x);
					range.maxX = Math.max(range.maxX, sample.x);
					range.minY = Math.min(range.minY, sample.y);
					range.maxY = Math.max(range.maxY, sample.y);
					range.samples += 1;

					await wait(CALIBRATION_SAMPLE_INTERVAL_MS);
				}
			} finally {
				setCalibratingStick(null);
			}

			const rawDeadzone = Number(values[`joystickDeadzone${suffix}`]);
			const minimumTravel = Math.max(
				Number.isFinite(rawDeadzone) ? rawDeadzone : 0,
				MIN_CALIBRATION_TRAVEL,
			);
			const travel = {
				xLow: neutral.x - range.minX,
				xHigh: range.maxX - neutral.x,
				yLow: neutral.y - range.minY,
				yHigh: range.maxY - neutral.y,
			};

			if (
				range.samples === 0 ||
				range.minX >= neutral.x ||
				range.maxX <= neutral.x ||
				range.minY >= neutral.y ||
				range.maxY <= neutral.y
			) {
				throw new Error(t('AddonsConfig:analog-calibration-invalid-range', {
					defaultValue: 'Calibration failed because the measured range did not cross the neutral position on both axes.',
				}));
			}

			if (
				travel.xLow < minimumTravel ||
				travel.xHigh < minimumTravel ||
				travel.yLow < minimumTravel ||
				travel.yHigh < minimumTravel
			) {
				throw new Error(t('AddonsConfig:analog-calibration-insufficient-range', {
					defaultValue: 'Stick did not travel far enough. Rotate the stick fully around the gate and try again.',
				}));
			}

			const minX = range.minX;
			const maxX = range.maxX;
			const minY = range.minY;
			const maxY = range.maxY;

			setFieldValue(`joystickMinX${suffix}`, minX);
			setFieldValue(`joystickMaxX${suffix}`, maxX);
			setFieldValue(`joystickMinY${suffix}`, minY);
			setFieldValue(`joystickMaxY${suffix}`, maxY);
			setFieldValue(`joystickCenterX${suffix}`, neutral.x);
			setFieldValue(`joystickCenterY${suffix}`, neutral.y);

			console.log('Calibration completed:', {
				range,
				finalCalibration: {
					minX,
					maxX,
					minY,
					maxY,
					neutralX: neutral.x,
					neutralY: neutral.y,
				},
			});

			const successKey = stick === 1
				? 'AddonsConfig:analog-calibration-success-stick-1'
				: 'AddonsConfig:analog-calibration-success-stick-2';
			const sampleRows = [
				`Neutral: X=${neutral.x}, Y=${neutral.y}`,
				`X: min=${minX}, max=${maxX}`,
				`Y: min=${minY}, max=${maxY}`,
				`Samples: ${range.samples}`,
			].join('\n');

			alert(
				t(successKey, {
					defaultValue: `Stick ${stick} calibration successful!`,
				}) + '\n\n' +
				t('AddonsConfig:analog-calibration-data', {
					defaultValue: 'Calibration data:',
				}) + '\n' +
				sampleRows + '\n\n' +
				t('AddonsConfig:analog-calibration-final-axis', {
					minX,
					maxX,
					minY,
					maxY,
					neutralX: neutral.x,
					neutralY: neutral.y,
					defaultValue: 'Final calibration: X min={{minX}}, neutral={{neutralX}}, max={{maxX}}; Y min={{minY}}, neutral={{neutralY}}, max={{maxY}}',
				}) + '\n\n' +
				t('AddonsConfig:analog-calibration-save-notice', {
					defaultValue: 'Please save configuration to apply calibration values.',
				})
			);
		} catch (err) {
			console.error(`Failed to calibrate joystick ${stick}`, err);
			alert(t('AddonsConfig:analog-calibration-failed', {
				error: err instanceof Error ? err.message : String(err),
				defaultValue: 'Calibration failed: {{error}}',
			}));
		}
	};

	const calibrationButtonLabel = (stick: 1 | 2, labelKey: string) => (
		calibratingStick === stick
			? t('AddonsConfig:analog-calibration-calibrating', { defaultValue: 'Calibrating...' })
			: t(labelKey)
	);

	return (
		<Section title={
			<a
				href="https://gp2040-ce.info/add-ons/analog"
				target="_blank"
				rel="noreferrer"
				className="text-reset text-decoration-none"
			>
				{t('AddonsConfig:analog-header-text')}
			</a>
		}
		>
			<div id="AnalogInputOptions" hidden={!values.AnalogInputEnabled}>
				<div className="alert alert-info" role="alert">
					{t('AddonsConfig:analog-warning')}
				</div>
				<div className="alert alert-success" role="alert">
					{t('AddonsConfig:analog-available-pins-text', {
						pins: availableAnalogPins.join(', '),
					})}
				</div>
					<Tabs
						defaultActiveKey="analog1Config"
						id="analogConfigTabs"
						className="mb-3 pb-0"
						fill
					>
						<Tab
							key="analog1Config"
							eventKey="analog1Config"
							title={t('AddonsConfig:analog-adc-1')}
						>
							<Row className="mb-3">
								<FormSelect
									label={t('AddonsConfig:analog-adc-1-pin-x-label')}
									name="analogAdc1PinX"
									className="form-select-sm"
									groupClassName="col-sm-3 mb-3"
									value={values.analogAdc1PinX}
									error={errors.analogAdc1PinX}
									isInvalid={Boolean(errors.analogAdc1PinX)}
									onChange={handleChange}
								>
									<AnalogPinOptions />
								</FormSelect>
								<FormSelect
									label={t('AddonsConfig:analog-adc-1-pin-y-label')}
									name="analogAdc1PinY"
									className="form-select-sm"
									groupClassName="col-sm-3 mb-3"
									value={values.analogAdc1PinY}
									error={errors.analogAdc1PinY}
									isInvalid={Boolean(errors.analogAdc1PinY)}
									onChange={handleChange}
								>
									<AnalogPinOptions />
								</FormSelect>
								<Row className="mb-3">
									<FormSelect
										label={t('AddonsConfig:analog-adc-1-mode-label')}
										name="analogAdc1Mode"
										className="form-select-sm"
										groupClassName="col-sm-3 mb-3"
										value={values.analogAdc1Mode}
										error={errors.analogAdc1Mode}
										isInvalid={Boolean(errors.analogAdc1Mode)}
										onChange={handleChange}
									>
										{ANALOG_STICK_MODES.map((o, i) => (
											<option key={`button-analogAdc1Mode-option-${i}`} value={o.value}>
												{o.label}
											</option>
										))}
									</FormSelect>
									<FormSelect
										label={t('AddonsConfig:analog-adc-1-invert-label')}
										name="analogAdc1Invert"
										className="form-select-sm"
										groupClassName="col-sm-3 mb-3"
										value={values.analogAdc1Invert}
										error={errors.analogAdc1Invert}
										isInvalid={Boolean(errors.analogAdc1Invert)}
										onChange={handleChange}
									>
										{INVERT_MODES.map((o, i) => (
											<option
												key={`button-analogAdc1Invert-option-${i}`}
												value={o.value}
											>
												{o.label}
											</option>
										))}
									</FormSelect>
								</Row>
								<Row className="mb-3">
									<FormControl
										type="number"
										label="Min X"
										name="joystickMinX"
										className="form-control-sm"
										groupClassName="col-sm-3 mb-3"
										value={values.joystickMinX}
										error={errors.joystickMinX}
										isInvalid={Boolean(errors.joystickMinX)}
										onChange={handleChange}
										min={0}
										max={4095}
									/>
									<FormControl
										type="number"
										label="Max X"
										name="joystickMaxX"
										className="form-control-sm"
										groupClassName="col-sm-3 mb-3"
										value={values.joystickMaxX}
										error={errors.joystickMaxX}
										isInvalid={Boolean(errors.joystickMaxX)}
										onChange={handleChange}
										min={0}
										max={4095}
									/>
									<FormControl
										type="number"
										label="Neutral X"
										name="joystickCenterX"
										className="form-control-sm"
										groupClassName="col-sm-3 mb-3"
										value={values.joystickCenterX}
										error={errors.joystickCenterX}
										isInvalid={Boolean(errors.joystickCenterX)}
										onChange={handleChange}
										min={0}
										max={4095}
									/>
									<FormControl
										type="number"
										label="Deadzone"
										name="joystickDeadzone"
										className="form-control-sm"
										groupClassName="col-sm-3 mb-3"
										value={values.joystickDeadzone}
										error={errors.joystickDeadzone}
										isInvalid={Boolean(errors.joystickDeadzone)}
										onChange={handleChange}
										min={0}
										max={4095}
									/>
								</Row>
								<Row className="mb-3">
									<FormControl
										type="number"
										label="Min Y"
										name="joystickMinY"
										className="form-control-sm"
										groupClassName="col-sm-3 mb-3"
										value={values.joystickMinY}
										error={errors.joystickMinY}
										isInvalid={Boolean(errors.joystickMinY)}
										onChange={handleChange}
										min={0}
										max={4095}
									/>
									<FormControl
										type="number"
										label="Max Y"
										name="joystickMaxY"
										className="form-control-sm"
										groupClassName="col-sm-3 mb-3"
										value={values.joystickMaxY}
										error={errors.joystickMaxY}
										isInvalid={Boolean(errors.joystickMaxY)}
										onChange={handleChange}
										min={0}
										max={4095}
									/>
									<FormControl
										type="number"
										label="Neutral Y"
										name="joystickCenterY"
										className="form-control-sm"
										groupClassName="col-sm-3 mb-3"
										value={values.joystickCenterY}
										error={errors.joystickCenterY}
										isInvalid={Boolean(errors.joystickCenterY)}
										onChange={handleChange}
										min={0}
										max={4095}
									/>
								</Row>
								<Row className="mb-3">
									<FormCheck
										label={t('AddonsConfig:analog-smoothing')}
										type="switch"
										id="Analog_smoothing"
										className="col-sm-3 ms-3"
										isInvalid={false}
										checked={Boolean(values.analog_smoothing)}
										onChange={(e) => {
											handleCheckbox('analog_smoothing');
											handleChange(e);
										}}
									/>
									<FormControl
										hidden={!values.analog_smoothing}
										type="number"
										label={t('AddonsConfig:smoothing-factor')}
										name="smoothing_factor"
										className="form-control-sm"
										groupClassName="col-sm-3 mb-3"
										value={values.smoothing_factor}
										error={errors.smoothing_factor}
										isInvalid={Boolean(errors.smoothing_factor)}
										onChange={handleChange}
										min={0}
										max={100}
									/>
								</Row>
								<Row className="mb-3">
									<FormCheck
										label={t('AddonsConfig:analog-force-circularity')}
										type="switch"
										id="Forced_circularity"
										className="col-sm-3 ms-3"
										isInvalid={false}
										checked={Boolean(values.forced_circularity)}
										onChange={(e) => {
											handleCheckbox('forced_circularity');
											handleChange(e);
										}}
									/>
									<FormControl
										hidden={!values.forced_circularity}
										type="number"
										label="Diagonal Compensation Strength (%)"
										name="diagonalCompensation"
										className="form-control-sm"
										groupClassName="col-sm-3 mb-3"
										value={values.diagonalCompensation}
										error={errors.diagonalCompensation}
										isInvalid={Boolean(errors.diagonalCompensation)}
										onChange={handleChange}
										min={0}
										max={100}
									/>
								</Row>
								<div className="d-flex align-items-center">
									<FormCheck
										label={t('AddonsConfig:analog-auto-calibrate')}
										type="switch"
										id="Auto_calibrate"
										className="col-sm-3 ms-3"
										isInvalid={false}
										checked={Boolean(values.auto_calibrate)}
										onChange={(e) => {
											handleCheckbox('auto_calibrate');
											handleChange(e);
										}}
									/>
									<button
										type="button"
										className="btn btn-sm btn-outline-secondary ms-2"
										disabled={Boolean(values.auto_calibrate) || calibratingStick !== null}
										onClick={() => runCalibration(1)}
									>
										{calibratingStick === 1 && (
											<Spinner
												as="span"
												animation="border"
												size="sm"
												role="status"
												aria-hidden="true"
												className="me-1"
											/>
										)}
										{calibrationButtonLabel(1, 'AddonsConfig:analog-calibrate-stick-1-button')}
									</button>
									<div className="ms-3 small text-muted" aria-live="polite">
										{calibratingStick === 1
											? t('AddonsConfig:analog-calibration-calibrating-status', {
												defaultValue: 'Calibrating min/max range...',
											})
											: `Neutral: X=${values.joystickCenterX}, Y=${values.joystickCenterY}`}
									</div>
								</div>
								{Boolean(values.auto_calibrate) && (
									<div className="alert alert-info mt-2 mb-3">
										<small>
											<strong>{t('AddonsConfig:analog-auto-calibration-enabled-stick-1')}</strong> {t('AddonsConfig:analog-calibration-auto-mode-instruction', { stick: '1' })}
										</small>
									</div>
								)}
								{!Boolean(values.auto_calibrate) && (
									<div className="alert alert-warning mt-2 mb-3">
										<small>
											<strong>{t('AddonsConfig:analog-manual-calibration-mode-stick-1')}</strong>
											<br />• {t('AddonsConfig:analog-calibration-manual-mode-instruction-1')}
											<br />• {t('AddonsConfig:analog-calibration-manual-mode-instruction-2')}
											<br />• {t('AddonsConfig:analog-calibration-manual-mode-instruction-3')}
											<br />• {t('AddonsConfig:analog-calibration-manual-mode-instruction-4')}
										</small>
									</div>
								)}
							</Row>
						</Tab>
						<Tab
							key="analog2Config"
							eventKey="analog2Config"
							title={t('AddonsConfig:analog-adc-2')}
						>
							<Row className="mb-3">
								<FormSelect
									label={t('AddonsConfig:analog-adc-2-pin-x-label')}
									name="analogAdc2PinX"
									className="form-select-sm"
									groupClassName="col-sm-3 mb-3"
									value={values.analogAdc2PinX}
									error={errors.analogAdc2PinX}
									isInvalid={Boolean(errors.analogAdc2PinX)}
									onChange={handleChange}
								>
									<AnalogPinOptions />
								</FormSelect>
								<FormSelect
									label={t('AddonsConfig:analog-adc-2-pin-y-label')}
									name="analogAdc2PinY"
									className="form-select-sm"
									groupClassName="col-sm-3 mb-3"
									value={values.analogAdc2PinY}
									error={errors.analogAdc2PinY}
									isInvalid={Boolean(errors.analogAdc2PinY)}
									onChange={handleChange}
								>
									<AnalogPinOptions />
								</FormSelect>
								<Row className="mb-3">
									<FormSelect
										label={t('AddonsConfig:analog-adc-2-mode-label')}
										name="analogAdc2Mode"
										className="form-select-sm"
										groupClassName="col-sm-3 mb-3"
										value={values.analogAdc2Mode}
										error={errors.analogAdc2Mode}
										isInvalid={Boolean(errors.analogAdc2Mode)}
										onChange={handleChange}
									>
										{ANALOG_STICK_MODES.map((o, i) => (
											<option key={`button-analogAdc2Mode-option-${i}`} value={o.value}>
												{o.label}
											</option>
										))}
									</FormSelect>
									<FormSelect
										label={t('AddonsConfig:analog-adc-2-invert-label')}
										name="analogAdc2Invert"
										className="form-select-sm"
										groupClassName="col-sm-3 mb-3"
										value={values.analogAdc2Invert}
										error={errors.analogAdc2Invert}
										isInvalid={Boolean(errors.analogAdc2Invert)}
										onChange={handleChange}
									>
										{INVERT_MODES.map((o, i) => (
											<option
												key={`button-analogAdc2Invert-option-${i}`}
												value={o.value}
											>
												{o.label}
											</option>
										))}
									</FormSelect>
								</Row>
								<Row className="mb-3">
									<FormControl
										type="number"
										label="Min X"
										name="joystickMinX2"
										className="form-control-sm"
										groupClassName="col-sm-3 mb-3"
										value={values.joystickMinX2}
										error={errors.joystickMinX2}
										isInvalid={Boolean(errors.joystickMinX2)}
										onChange={handleChange}
										min={0}
										max={4095}
									/>
									<FormControl
										type="number"
										label="Max X"
										name="joystickMaxX2"
										className="form-control-sm"
										groupClassName="col-sm-3 mb-3"
										value={values.joystickMaxX2}
										error={errors.joystickMaxX2}
										isInvalid={Boolean(errors.joystickMaxX2)}
										onChange={handleChange}
										min={0}
										max={4095}
									/>
									<FormControl
										type="number"
										label="Neutral X"
										name="joystickCenterX2"
										className="form-control-sm"
										groupClassName="col-sm-3 mb-3"
										value={values.joystickCenterX2}
										error={errors.joystickCenterX2}
										isInvalid={Boolean(errors.joystickCenterX2)}
										onChange={handleChange}
										min={0}
										max={4095}
									/>
									<FormControl
										type="number"
										label="Deadzone"
										name="joystickDeadzone2"
										className="form-control-sm"
										groupClassName="col-sm-3 mb-3"
										value={values.joystickDeadzone2}
										error={errors.joystickDeadzone2}
										isInvalid={Boolean(errors.joystickDeadzone2)}
										onChange={handleChange}
										min={0}
										max={4095}
									/>
								</Row>
								<Row className="mb-3">
									<FormControl
										type="number"
										label="Min Y"
										name="joystickMinY2"
										className="form-control-sm"
										groupClassName="col-sm-3 mb-3"
										value={values.joystickMinY2}
										error={errors.joystickMinY2}
										isInvalid={Boolean(errors.joystickMinY2)}
										onChange={handleChange}
										min={0}
										max={4095}
									/>
									<FormControl
										type="number"
										label="Max Y"
										name="joystickMaxY2"
										className="form-control-sm"
										groupClassName="col-sm-3 mb-3"
										value={values.joystickMaxY2}
										error={errors.joystickMaxY2}
										isInvalid={Boolean(errors.joystickMaxY2)}
										onChange={handleChange}
										min={0}
										max={4095}
									/>
									<FormControl
										type="number"
										label="Neutral Y"
										name="joystickCenterY2"
										className="form-control-sm"
										groupClassName="col-sm-3 mb-3"
										value={values.joystickCenterY2}
										error={errors.joystickCenterY2}
										isInvalid={Boolean(errors.joystickCenterY2)}
										onChange={handleChange}
										min={0}
										max={4095}
									/>
								</Row>
								<Row className="mb-3">
									<FormCheck
										label={t('AddonsConfig:analog-smoothing')}
										type="switch"
										id="Analog_smoothing2"
										className="col-sm-3 ms-3"
										isInvalid={false}
										checked={Boolean(values.analog_smoothing2)}
										onChange={(e) => {
											handleCheckbox('analog_smoothing2');
											handleChange(e);
										}}
									/>
									<FormControl
										hidden={!values.analog_smoothing2}
										type="number"
										label={t('AddonsConfig:smoothing-factor')}
										name="smoothing_factor2"
										className="form-control-sm"
										groupClassName="col-sm-3 mb-3"
										value={values.smoothing_factor2}
										error={errors.smoothing_factor2}
										isInvalid={Boolean(errors.smoothing_factor2)}
										onChange={handleChange}
										min={0}
										max={100}
									/>
								</Row>
								<Row className="mb-3">
									<FormCheck
										label={t('AddonsConfig:analog-force-circularity')}
										type="switch"
										id="Forced_circularity2"
										className="col-sm-3 ms-3"
										isInvalid={false}
										checked={Boolean(values.forced_circularity2)}
										onChange={(e) => {
											handleCheckbox('forced_circularity2');
											handleChange(e);
										}}
									/>
									<FormControl
										hidden={!values.forced_circularity2}
										type="number"
										label="Diagonal Compensation Strength (%)"
										name="diagonalCompensation2"
										className="form-control-sm"
										groupClassName="col-sm-3 mb-3"
										value={values.diagonalCompensation2}
										error={errors.diagonalCompensation2}
										isInvalid={Boolean(errors.diagonalCompensation2)}
										onChange={handleChange}
										min={0}
										max={100}
									/>
								</Row>
								<div className="d-flex align-items-center">
									<FormCheck
										label={t('AddonsConfig:analog-auto-calibrate')}
										type="switch"
										id="Auto_calibrate2"
										className="col-sm-3 ms-3"
										isInvalid={false}
										checked={Boolean(values.auto_calibrate2)}
										onChange={(e) => {
											handleCheckbox('auto_calibrate2');
											handleChange(e);
										}}
									/>
									<button
										type="button"
										className="btn btn-sm btn-outline-secondary ms-2"
										disabled={Boolean(values.auto_calibrate2) || calibratingStick !== null}
										onClick={() => runCalibration(2)}
									>
										{calibratingStick === 2 && (
											<Spinner
												as="span"
												animation="border"
												size="sm"
												role="status"
												aria-hidden="true"
												className="me-1"
											/>
										)}
										{calibrationButtonLabel(2, 'AddonsConfig:analog-calibrate-stick-2-button')}
									</button>
									<div className="ms-3 small text-muted" aria-live="polite">
										{calibratingStick === 2
											? t('AddonsConfig:analog-calibration-calibrating-status', {
												defaultValue: 'Calibrating min/max range...',
											})
											: `Neutral: X=${values.joystickCenterX2}, Y=${values.joystickCenterY2}`}
									</div>
								</div>
								{Boolean(values.auto_calibrate2) && (
									<div className="alert alert-info mt-2 mb-3">
										<small>
											<strong>{t('AddonsConfig:analog-auto-calibration-enabled-stick-2')}</strong> {t('AddonsConfig:analog-calibration-auto-mode-instruction', { stick: '2' })}
										</small>
									</div>
								)}
								{!Boolean(values.auto_calibrate2) && (
									<div className="alert alert-warning mt-2 mb-3">
										<small>
											<strong>{t('AddonsConfig:analog-manual-calibration-mode-stick-2')}</strong>
											<br />• {t('AddonsConfig:analog-calibration-manual-mode-instruction-1')}
											<br />• {t('AddonsConfig:analog-calibration-manual-mode-instruction-2')}
											<br />• {t('AddonsConfig:analog-calibration-manual-mode-instruction-3')}
											<br />• {t('AddonsConfig:analog-calibration-manual-mode-instruction-4')}
										</small>
									</div>
								)}
							</Row>
						</Tab>
					</Tabs>
			</div>
			<FormCheck
				label={t('Common:switch-enabled')}
				type="switch"
				id="AnalogInputButton"
				reverse
				isInvalid={false}
				checked={Boolean(values.AnalogInputEnabled)}
				onChange={(e) => {
					handleCheckbox('AnalogInputEnabled');
					handleChange(e);
				}}
			/>
		</Section>
	);
};

export default Analog;
