import { useContext, useState } from 'react';
import { Alert, Button, FormCheck, FormLabel } from 'react-bootstrap';
import { Trans, useTranslation } from 'react-i18next';
import { NavLink } from 'react-router-dom';
import * as yup from 'yup';

import Section from '../Components/Section';
import { AppContext } from '../Contexts/AppContext';
import { AddonPropTypes } from '../Pages/AddonsConfigPage';
import WebApi from '../Services/WebApi';

export const gyroScheme = {
	GyroAddonEnabled: yup.number().required().label('Gyroscope Add-On Enabled'),
	gyroAddress: yup.number().oneOf([0, 0x6a, 0x6b]).label('Gyroscope I2C Address'),
	gyroAccelOffsetX: yup.number(),
	gyroAccelOffsetY: yup.number(),
	gyroAccelOffsetZ: yup.number(),
	gyroGyroOffsetX: yup.number(),
	gyroGyroOffsetY: yup.number(),
	gyroGyroOffsetZ: yup.number(),
};

export const gyroState = {
	GyroAddonEnabled: 0,
	gyroAddress: 0,
	gyroAccelOffsetX: 0,
	gyroAccelOffsetY: 0,
	gyroAccelOffsetZ: 0,
	gyroGyroOffsetX: 0,
	gyroGyroOffsetY: 0,
	gyroGyroOffsetZ: 0,
};

const GYRO_ADDRESS_OPTIONS = [
	{ label: 'gyro-address-auto', value: 0 },
	{ label: 'gyro-address-6a', value: 0x6a },
	{ label: 'gyro-address-6b', value: 0x6b },
];

const GYRO_OFFSET_FIELDS = [
	{ name: 'gyroAccelOffsetX', label: 'gyro-offset-accel-x' },
	{ name: 'gyroAccelOffsetY', label: 'gyro-offset-accel-y' },
	{ name: 'gyroAccelOffsetZ', label: 'gyro-offset-accel-z' },
	{ name: 'gyroGyroOffsetX', label: 'gyro-offset-gyro-x' },
	{ name: 'gyroGyroOffsetY', label: 'gyro-offset-gyro-y' },
	{ name: 'gyroGyroOffsetZ', label: 'gyro-offset-gyro-z' },
] as const;

const Gyro = ({ values, handleChange, handleCheckbox, setFieldValue }: AddonPropTypes) => {
	const { t } = useTranslation();
	const { getAvailablePeripherals } = useContext(AppContext);
	const i2cAvailable = getAvailablePeripherals('i2c');
	const [isMeasuring, setIsMeasuring] = useState(false);
	const [measureMessage, setMeasureMessage] = useState('');
	const [measureError, setMeasureError] = useState(false);

	const measureOffsets = async () => {
		setIsMeasuring(true);
		setMeasureMessage('');
		setMeasureError(false);

		const result = await WebApi.measureGyroOffsets({
			gyroAddress: Number(values.gyroAddress),
		});

		if (result?.success) {
			setFieldValue('gyroAccelOffsetX', result.accelOffsetX);
			setFieldValue('gyroAccelOffsetY', result.accelOffsetY);
			setFieldValue('gyroAccelOffsetZ', result.accelOffsetZ);
			setFieldValue('gyroGyroOffsetX', result.gyroOffsetX);
			setFieldValue('gyroGyroOffsetY', result.gyroOffsetY);
			setFieldValue('gyroGyroOffsetZ', result.gyroOffsetZ);
			setMeasureMessage(t('AddonsConfig:gyro-offset-measure-success'));
		} else {
			setMeasureError(true);
			setMeasureMessage(
				t('AddonsConfig:gyro-offset-measure-failed', {
					error: result?.error || 'unknown error',
				}),
			);
		}

		setIsMeasuring(false);
	};

	return (
		<Section title={t('AddonsConfig:gyro-header-text')}>
			<div id="GyroAddonOptions" hidden={!(values.GyroAddonEnabled && i2cAvailable)}>
				<div className="alert alert-info" role="alert">
					<Trans ns="AddonsConfig" i18nKey="gyro-i2c-config-text">
						The SDA and SCL pins and Speed are configured in{' '}
						<a href="../peripheral-mapping" className="alert-link">
							Peripheral Mapping
						</a>
					</Trans>
				</div>
				<div className="row mb-3">
					<div className="col-sm-12 col-md-6 col-lg-3">
						<label className="form-label" htmlFor="gyroAddress">
							{t('AddonsConfig:gyro-address-label')}
						</label>
						<select
							className="form-select"
							id="gyroAddress"
							name="gyroAddress"
							value={values.gyroAddress}
							onChange={handleChange}
						>
							{GYRO_ADDRESS_OPTIONS.map((option) => (
								<option key={`gyro-address-${option.value}`} value={option.value}>
									{t(`AddonsConfig:${option.label}`)}
								</option>
							))}
						</select>
					</div>
				</div>
				<div className="row mb-3">
					{GYRO_OFFSET_FIELDS.map((field) => (
						<div className="col-sm-6 col-md-4 col-lg-2" key={field.name}>
							<label className="form-label" htmlFor={field.name}>
								{t(`AddonsConfig:${field.label}`)}
							</label>
							<input
								className="form-control"
								id={field.name}
								name={field.name}
								type="number"
								value={values[field.name]}
								onChange={handleChange}
							/>
						</div>
					))}
				</div>
				<div className="row mb-3">
					<div className="col-sm-12">
						<Button
							type="button"
							variant="secondary"
							disabled={isMeasuring}
							onClick={measureOffsets}
						>
							{isMeasuring
								? t('AddonsConfig:gyro-offset-measuring-button')
								: t('AddonsConfig:gyro-offset-measure-button')}
						</Button>
						<div className="form-text">
							{t('AddonsConfig:gyro-offset-save-notice')}
						</div>
						{measureMessage ? (
							<Alert
								className="mt-2 mb-0"
								variant={measureError ? 'danger' : 'success'}
							>
								{measureMessage}
							</Alert>
						) : null}
					</div>
				</div>
			</div>
			{i2cAvailable ? (
				<FormCheck
					label={t('Common:switch-enabled')}
					type="switch"
					id="GyroAddonButton"
					reverse
					isInvalid={false}
					checked={Boolean(values.GyroAddonEnabled) && i2cAvailable}
					onChange={(e) => {
						handleCheckbox('GyroAddonEnabled');
						handleChange(e);
					}}
				/>
			) : (
				<FormLabel>
					<Trans
						ns="PeripheralMapping"
						i18nKey="peripheral-toggle-unavailable"
						values={{ name: 'I2C' }}
					>
						<NavLink to="/peripheral-mapping">
							{t('PeripheralMapping:header-text')}
						</NavLink>
					</Trans>
				</FormLabel>
			)}
		</Section>
	);
};

export default Gyro;
