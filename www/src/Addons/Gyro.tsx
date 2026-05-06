import { useContext } from 'react';
import { FormCheck, FormLabel } from 'react-bootstrap';
import { Trans, useTranslation } from 'react-i18next';
import { NavLink } from 'react-router-dom';
import * as yup from 'yup';

import Section from '../Components/Section';
import { AppContext } from '../Contexts/AppContext';
import { AddonPropTypes } from '../Pages/AddonsConfigPage';

export const gyroScheme = {
	GyroAddonEnabled: yup.number().required().label('Gyroscope Add-On Enabled'),
	gyroAddress: yup.number().oneOf([0, 0x6a, 0x6b]).label('Gyroscope I2C Address'),
	gyroAccelAxisX: yup.number().oneOf([-3, -2, -1, 1, 2, 3]),
	gyroAccelAxisY: yup.number().oneOf([-3, -2, -1, 1, 2, 3]),
	gyroAccelAxisZ: yup.number().oneOf([-3, -2, -1, 1, 2, 3]),
	gyroGyroAxisX: yup.number().oneOf([-3, -2, -1, 1, 2, 3]),
	gyroGyroAxisY: yup.number().oneOf([-3, -2, -1, 1, 2, 3]),
	gyroGyroAxisZ: yup.number().oneOf([-3, -2, -1, 1, 2, 3]),
};

export const gyroState = {
	GyroAddonEnabled: 0,
	gyroAddress: 0,
	gyroAccelAxisX: 1,
	gyroAccelAxisY: 2,
	gyroAccelAxisZ: 3,
	gyroGyroAxisX: 1,
	gyroGyroAxisY: 2,
	gyroGyroAxisZ: 3,
};

const GYRO_ADDRESS_OPTIONS = [
	{ label: 'gyro-address-auto', value: 0 },
	{ label: 'gyro-address-6a', value: 0x6a },
	{ label: 'gyro-address-6b', value: 0x6b },
];

const Gyro = ({ values, handleChange, handleCheckbox }: AddonPropTypes) => {
	const { t } = useTranslation();
	const { getAvailablePeripherals } = useContext(AppContext);
	const i2cAvailable = getAvailablePeripherals('i2c');

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
